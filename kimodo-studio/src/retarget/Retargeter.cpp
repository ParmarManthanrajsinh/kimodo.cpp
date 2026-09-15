#include "retarget/Retargeter.h"

#include "animation/Skeleton.h"
#include "raymath.h"
#include "retarget/SkeletonProfile.h"

#include <cmath>

namespace studio {
namespace {

// --- local quat helpers (xyzw), independent of raymath layout ---
Quaternion qNorm(Quaternion q) {
    return QuaternionNormalize(q);
}

Quaternion qSlerpFrac(Quaternion q, float w) {
    // slerp(identity, q, w): w=1 full motion, w=0 rest.
    if (w >= 1.0f) {
        return qNorm(q);
    }
    if (w <= 0.0f) {
        return Quaternion{0, 0, 0, 1};
    }
    return QuaternionNormalize(QuaternionSlerp(Quaternion{0, 0, 0, 1}, q, w));
}

float restLength(const std::array<float, 3>& o) {
    return std::sqrt(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
}

} // namespace

BasisConvert BasisConvert::yUpToZUp() {
    BasisConvert b;
    b.m[0][0] = 1;
    b.m[0][1] = 0;
    b.m[0][2] = 0;
    b.m[1][0] = 0;
    b.m[1][1] = 0;
    b.m[1][2] = 1;
    b.m[2][0] = 0;
    b.m[2][1] = -1;
    b.m[2][2] = 0;
    return b;
}

std::array<float, 3> BasisConvert::applyPos(const std::array<float, 3>& v,
                                            float scale) const {
    return {scale * (m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2]),
            scale * (m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2]),
            scale * (m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2])};
}

std::array<float, 4> BasisConvert::applyQuat(const std::array<float, 4>& q) const {
    // q' = C * q * C^-1 via 3x3 matrices.
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    const float xx = x * x, yy = y * y, zz = z * z;
    const float xy = x * y, xz = x * z, yz = y * z;
    const float wx = w * x, wy = w * y, wz = w * z;
    float r[3][3] = {{1 - 2 * (yy + zz), 2 * (xy - wz), 2 * (xz + wy)},
                     {2 * (xy + wz), 1 - 2 * (xx + zz), 2 * (yz - wx)},
                     {2 * (xz - wy), 2 * (yz + wx), 1 - 2 * (xx + yy)}};
    // R' = C * R * Ct.
    float tmp[3][3] = {};
    float out[3][3] = {};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            tmp[i][j] = m[i][0] * r[0][j] + m[i][1] * r[1][j] + m[i][2] * r[2][j];
        }
    }
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            out[i][j] = tmp[i][0] * m[j][0] + tmp[i][1] * m[j][1] + tmp[i][2] * m[j][2];
        }
    }
    // mat3 -> xyzw.
    const float t = out[0][0] + out[1][1] + out[2][2];
    std::array<float, 4> o = {0, 0, 0, 1};
    if (t > 0) {
        const float s = 2 * std::sqrt(t + 1);
        o = {(out[2][1] - out[1][2]) / s, (out[0][2] - out[2][0]) / s,
             (out[1][0] - out[0][1]) / s, 0.25f * s};
    } else if (out[0][0] > out[1][1] && out[0][0] > out[2][2]) {
        const float s = 2 * std::sqrt(1 + out[0][0] - out[1][1] - out[2][2]);
        o = {0.25f * s, (out[0][1] + out[1][0]) / s, (out[0][2] + out[2][0]) / s,
             (out[2][1] - out[1][2]) / s};
    } else if (out[1][1] > out[2][2]) {
        const float s = 2 * std::sqrt(1 + out[1][1] - out[0][0] - out[2][2]);
        o = {(out[0][1] + out[1][0]) / s, 0.25f * s, (out[1][2] + out[2][1]) / s,
             (out[0][2] - out[2][0]) / s};
    } else {
        const float s = 2 * std::sqrt(1 + out[2][2] - out[0][0] - out[1][1]);
        o = {(out[0][2] + out[2][0]) / s, (out[1][2] + out[2][1]) / s, 0.25f * s,
             (out[1][0] - out[0][1]) / s};
    }
    const float n = std::sqrt(o[0] * o[0] + o[1] * o[1] + o[2] * o[2] + o[3] * o[3]);
    if (n > 1e-9f) {
        o[0] /= n;
        o[1] /= n;
        o[2] /= n;
        o[3] /= n;
    }
    return o;
}

BoneMap Retargeter::autoMap(const SkeletonProfile& target) {
    BoneMap map;
    for (const auto& [tgt, src] : target.defaultMap) {
        map[tgt] = src;
    }
    return map;
}

std::vector<std::string> Retargeter::unmapped(const SkeletonProfile& target,
                                              const BoneMap& map) {
    std::vector<std::string> out;
    for (const std::string& j : target.joints) {
        if (j == "root") {
            continue; // translation carrier by design, never mapped
        }
        const auto it = map.find(j);
        if (it == map.end() || it->second.empty() || it->second == "(none)") {
            out.push_back(j);
        }
    }
    return out;
}

std::vector<std::string> Retargeter::missingChains(const SkeletonProfile& target,
                                                   const BoneMap& map) {
    std::vector<std::string> missing;
    for (const ChainDef& chain : target.chains) {
        bool any = false;
        for (const std::string& t : chain.target) {
            const auto it = map.find(t);
            if (it != map.end() && !it->second.empty() && it->second != "(none)") {
                any = true;
                break;
            }
        }
        if (!any) {
            std::string bones;
            for (const std::string& t : chain.target) {
                if (!bones.empty()) {
                    bones += ",";
                }
                bones += t;
            }
            missing.push_back("chain '" + chain.name + "' has no mapped bones (" + bones +
                              ")");
        }
    }
    return missing;
}

namespace {

// One target bone's motion assignment for a frame batch.
struct TargetPlan {
    int src = -1;      // source joint index (-1 = hold rest)
    float weight = 1;  // span fraction (slerp from identity)
    int chain = -1;    // chain index, -1 = legacy/direct
    bool holdRest = true;
};

int findIndex(const std::vector<std::string>& names, const std::string& n) {
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == n) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

Quaternion toRayQuat(const std::array<float, 4>& q) {
    return Quaternion{q[0], q[1], q[2], q[3]};
}

} // namespace

bool Retargeter::retarget(const Animation& source, const SkeletonProfile& target,
                         const BoneMap& map, const Options& opts, Animation& out,
                         std::string& error, RetargetReport* report) {
    if (source.empty()) {
        error = "source animation empty";
        return false;
    }
    const int T = static_cast<int>(target.joints.size());
    const int S = source.joints;
    if (T <= 0 || S <= 0) {
        error = "empty topology";
        return false;
    }
    const std::vector<std::string> missing = missingChains(target, map);
    if (!missing.empty()) {
        error = "unmapped chains: ";
        for (size_t i = 0; i < missing.size(); ++i) {
            if (i > 0) {
                error += "; ";
            }
            error += missing[i];
        }
        return false;
    }

    auto srcIndex = [&source](const std::string& name) -> int {
        return findIndex(source.jointNames, name);
    };

    Animation result;
    result.frames = source.frames;
    result.joints = T;
    result.fps = source.fps;
    result.skeletonName = target.id;
    result.jointNames = target.joints;
    result.parents = target.parents;
    result.offsets.resize(T, {0, 0, 0});
    result.localRotationsXyzw.assign(static_cast<size_t>(source.frames) * T * 4, 0.0f);
    result.rootPositions.resize(static_cast<size_t>(source.frames) * 3);

    const bool useBind = target.hasBind && static_cast<int>(target.offsets.size()) == T &&
                         static_cast<int>(target.restLocal.size()) == T;
    if (useBind) {
        result.offsets = target.offsets;
    }

    // Target reference (STEP 1/2): one shared bind pose, validated UE5
    // hierarchy. No yaw normalization, no ad-hoc rest edits (STEP 5):
    // the bind pose stays exactly as measured.
    TargetReference tgtRef;
    std::vector<Quaternion> tgtRestWorld(T, {0, 0, 0, 1});
    if (useBind && !target.chains.empty()) {
        if (!buildTargetReference(target, tgtRef, error)) {
            return false;
        }
        for (int t = 0; t < T; ++t) {
            tgtRestWorld[t] = {tgtRef.worldRot[t][0], tgtRef.worldRot[t][1],
                               tgtRef.worldRot[t][2], tgtRef.worldRot[t][3]};
        }
    }

    // Build per-target motion plan: chain spans share source motion by
    // rest-length weights (never duplicates a full source rotation).
    std::vector<TargetPlan> plan(T);
    std::vector<bool> planned(T, false);
    for (size_t ci = 0; ci < target.chains.size(); ++ci) {
        const ChainDef& chain = target.chains[ci];
        ChainParams params;
        const auto pit = opts.chains.find(chain.name);
        if (pit != opts.chains.end()) {
            params = pit->second;
        }
        // Ordered target indices present in this profile.
        std::vector<int> order;
        for (const std::string& tn : chain.target) {
            const int ti = findIndex(target.joints, tn);
            if (ti >= 0) {
                order.push_back(ti);
            }
        }
        if (order.empty()) {
            continue;
        }
        if (!params.enabled) {
            continue; // hold rest (plan defaults)
        }
        // Resolve mapped source per target.
        std::vector<int> srcOf(order.size(), -1);
        for (size_t k = 0; k < order.size(); ++k) {
            const auto it = map.find(target.joints[order[k]]);
            if (it != map.end() && !it->second.empty() && it->second != "(none)") {
                srcOf[k] = srcIndex(it->second);
            }
        }
        // Group consecutive targets sharing one source -> span.
        size_t k = 0;
        while (k < order.size()) {
            if (srcOf[k] < 0) {
                ++k; // unmapped in chain: hold rest
                continue;
            }
            size_t e = k;
            while (e + 1 < order.size() && srcOf[e + 1] == srcOf[k]) {
                ++e;
            }
            float total = 0.0f;
            for (size_t m = k; m <= e; ++m) {
                total += useBind ? restLength(result.offsets[order[m]]) : 1.0f;
            }
            for (size_t m = k; m <= e; ++m) {
                const int t = order[m];
                float w = 1.0f;
                if (e > k) {
                    const float len =
                        useBind ? restLength(result.offsets[t]) : 1.0f;
                    w = (total > 1e-9f) ? len / total : 1.0f / (e - k + 1);
                }
                w *= params.motionScale;
                if (w < 0.0f) {
                    w = 0.0f;
                }
                if (w > 1.5f) {
                    w = 1.5f;
                }
                plan[t].src = srcOf[k];
                plan[t].weight = w;
                plan[t].chain = static_cast<int>(ci);
                plan[t].holdRest = false;
            }
            k = e + 1;
        }
    }
    // Root fallback: an unmapped root follows its first mapped child's
    // source so facing/travel track the clip (translation is separate).
    for (int t = 0; t < T; ++t) {
        if (target.parents[t] >= 0 || !plan[t].holdRest) {
            continue;
        }
        for (int c = 0; c < T; ++c) {
            if (target.parents[c] == t && !plan[c].holdRest && plan[c].src >= 0) {
                plan[t].src = plan[c].src;
                plan[t].weight = 1.0f;
                plan[t].holdRest = false;
                plan[t].chain = plan[c].chain;
                break;
            }
        }
    }
    // Legacy fallback for joints in no chain (unity/blender profiles).
    if (target.chains.empty()) {
        for (int t = 0; t < T; ++t) {
            const auto it = map.find(target.joints[t]);
            if (it != map.end() && !it->second.empty() && it->second != "(none)") {
                const int s = srcIndex(it->second);
                if (s >= 0) {
                    plan[t].src = s;
                    plan[t].weight = 1.0f;
                    plan[t].holdRest = false;
                }
            }
        }
    }

    std::vector<Vector3> srcPos;
    std::vector<Quaternion> srcWorld;
    std::vector<Quaternion> outWorld(T);
    for (int f = 0; f < source.frames; ++f) {
        const float* srcRots =
            source.localRotationsXyzw.data() + static_cast<size_t>(f) * S * 4;
        const float* srcRoot =
            source.rootPositions.data() + static_cast<size_t>(f) * 3;
        float* dstRots =
            result.localRotationsXyzw.data() + static_cast<size_t>(f) * T * 4;
        const bool needWorld = useBind || !target.chains.empty();
        if (needWorld) {
            Skeleton::forwardKinematicsFull(srcRots, srcRoot, source.parents,
                                            source.offsets, srcPos, srcWorld);
        }
        for (int t = 0; t < T; ++t) {
            const TargetPlan& pl = plan[t];
            const bool isRoot = target.parents[t] < 0;
            float* q = dstRots + t * 4;
            Quaternion tw;
            if (isRoot) {
                // Root keeps converted source world: exact facing/travel.
                if (pl.src >= 0 && pl.src < S &&
                    pl.src < static_cast<int>(srcWorld.size())) {
                    const auto& s = srcWorld[pl.src];
                    const auto c = opts.basis.applyQuat({s.x, s.y, s.z, s.w});
                    tw = QuaternionNormalize({c[0], c[1], c[2], c[3]});
                } else if (!tgtRestWorld.empty()) {
                    tw = tgtRestWorld[t];
                } else {
                    tw = {0, 0, 0, 1};
                }
            } else if (!pl.holdRest && pl.src >= 0 && pl.src < S) {
                // Reference-relative delta (STEP 4/6/7):
                //   sourceDelta = L_anim * inv(L_ref), L_ref = identity
                //     (SOMA convention: Kimodo emits absolute locals, no
                //     baked rest rotations; asserted by construction).
                //   tw = targetWorldRef * C(sourceDelta), span-fractioned.
                // Local (not world) deltas: a child world contains parent
                // motion, which would cancel against the transferred parent
                // and drop span motion. Root excluded (unmapped carrier,
                // keeps converted source world for facing/travel).
                const float* sl = srcRots + pl.src * 4;
                Quaternion delta{sl[0], sl[1], sl[2], sl[3]};
                // inv(L_ref) with L_ref = identity: explicit no-op documenting
                // the convention; replace with real ref when core provides one.
                delta = qNorm(delta);
                const auto cl = opts.basis.applyQuat(
                    {delta.x, delta.y, delta.z, delta.w});
                Quaternion moved =
                    qSlerpFrac({cl[0], cl[1], cl[2], cl[3]}, pl.weight);
                if (useBind) {
                    tw = QuaternionNormalize(
                        QuaternionMultiply(tgtRestWorld[t], moved));
                } else {
                    // Legacy copy path (no bind data): converted source local.
                    tw = QuaternionNormalize(moved);
                }
            } else if (useBind) {
                tw = tgtRestWorld[t]; // hold rest
            } else {
                tw = {0, 0, 0, 1};
            }
            outWorld[t] = tw;
            Quaternion pw = {0, 0, 0, 1};
            const int p = target.parents[t];
            if (p >= 0 && p < T) {
                pw = outWorld[p];
            }
            Quaternion lq = QuaternionNormalize(
                QuaternionMultiply(QuaternionInvert(pw), tw));
            q[0] = lq.x;
            q[1] = lq.y;
            q[2] = lq.z;
            q[3] = lq.w;
        }
        const auto rp = opts.basis.applyPos(
            {srcRoot[0], srcRoot[1], srcRoot[2]}, opts.rootScale);
        float* dstRoot = result.rootPositions.data() + static_cast<size_t>(f) * 3;
        dstRoot[0] = rp[0];
        dstRoot[1] = rp[1];
        dstRoot[2] = rp[2];
    }

    // Optional analytic two-bone leg IK (feet keep source contact).
    if (opts.legIK && !target.chains.empty()) {
        for (const ChainDef& chain : target.chains) {
            const bool isLeg = chain.name == "LeftLeg" || chain.name == "RightLeg";
            if (!isLeg || chain.target.size() < 3) {
                continue;
            }
            ChainParams params;
            const auto pit = opts.chains.find(chain.name);
            if (pit != opts.chains.end()) {
                params = pit->second;
            }
            if (!params.enabled) {
                continue;
            }
            const int thigh = findIndex(target.joints, chain.target[0]);
            const int calf = findIndex(target.joints, chain.target[1]);
            const int foot = findIndex(target.joints, chain.target[2]);
            if (thigh < 0 || calf < 0 || foot < 0) {
                continue;
            }
            // Foot targets: source foot motion RELATIVE TO SOURCE HIP,
            // applied to the target rest leg vector. Raw source foot
            // positions double-count root motion (hip drops AND target
            // drops); hip-relative deltas keep feet planted on crouch and
            // are a no-op on identity input. Frame 0 is the rest reference.
            std::vector<std::array<float, 3>> targets;
            targets.reserve(source.frames);
            {
                std::vector<Vector3> sp;
                std::vector<Quaternion> sw;
                int sFoot = -1;
                const auto fit = map.find(chain.target[2]);
                if (fit != map.end()) {
                    sFoot = srcIndex(fit->second);
                }
                int sHip = -1;
                for (int s = 0; s < S; ++s) {
                    if (source.parents[s] < 0) {
                        sHip = s;
                        break;
                    }
                }
                std::vector<Vector3> wantF(source.frames, {0, 0, 0});
                std::vector<Vector3> hipF(source.frames, {0, 0, 0});
                for (int f = 0; f < source.frames; ++f) {
                    const float* sr =
                        source.localRotationsXyzw.data() + static_cast<size_t>(f) * S * 4;
                    const float* st =
                        source.rootPositions.data() + static_cast<size_t>(f) * 3;
                    Skeleton::forwardKinematicsFull(sr, st, source.parents,
                                                    source.offsets, sp, sw);
                    if (sFoot >= 0 && sFoot < static_cast<int>(sp.size())) {
                        wantF[f] = sp[sFoot];
                    } else if (!sp.empty()) {
                        wantF[f] = sp.back();
                    }
                    if (sHip >= 0 && sHip < static_cast<int>(sp.size())) {
                        hipF[f] = sp[sHip];
                    }
                }
                // Target rest leg vector (translation cancels in subtraction).
                std::vector<float> restFlat(static_cast<size_t>(T) * 4);
                for (int t = 0; t < T; ++t) {
                    restFlat[t * 4] = target.restLocal[t][0];
                    restFlat[t * 4 + 1] = target.restLocal[t][1];
                    restFlat[t * 4 + 2] = target.restLocal[t][2];
                    restFlat[t * 4 + 3] = target.restLocal[t][3];
                }
                const float org[3] = {0, 0, 0};
                std::vector<Vector3> rp;
                std::vector<Quaternion> rr;
                Skeleton::forwardKinematicsFull(restFlat.data(), org, target.parents,
                                                result.offsets, rp, rr);
                const Vector3 restVec =
                    Vector3Subtract(rp[foot], rp[thigh]);
                // Leg-length normalization: hip-relative source deltas are in
                // source units; scale by target/source leg ratio so a crouch
                // that plants source feet also plants target feet across
                // different proportions (SOMA thigh 0.13 vs UE5 0.42).
                float legRatio = 1.0f;
                {
                    // Segment lengths, not parent offsets: thigh segment =
                    // offset[mapped calf source], calf segment = offset[mapped
                    // foot source] (sFoot resolved above).
                    int sCalf = -1;
                    const auto t1 = map.find(chain.target[1]);
                    if (t1 != map.end()) {
                        sCalf = srcIndex(t1->second);
                    }
                    if (sCalf >= 0 && sFoot >= 0 &&
                        sCalf < static_cast<int>(source.offsets.size()) &&
                        sFoot < static_cast<int>(source.offsets.size())) {
                        const auto& oc = source.offsets[sCalf];
                        const auto& of = source.offsets[sFoot];
                        const float sLen =
                            std::sqrt(oc[0] * oc[0] + oc[1] * oc[1] + oc[2] * oc[2]) +
                            std::sqrt(of[0] * of[0] + of[1] * of[1] + of[2] * of[2]);
                        if (sLen > 1e-6f) {
                            legRatio = (restLength(result.offsets[calf]) +
                                        restLength(result.offsets[foot])) /
                                       sLen;
                        }
                    }
                }
                if (std::getenv("KIMODO_IK_DEBUG")) {
                    const Vector3 r0 = Vector3Subtract(wantF[0], hipF[0]);
                    const Vector3 r1 = Vector3Subtract(wantF[source.frames - 1],
                                                       hipF[source.frames - 1]);
                    std::printf("ikdbg %s legRatio=%.3f a+b=%.3f restVec=%.3f,%.3f,%.3f rel0=%.3f,%.3f,%.3f rel1=%.3f,%.3f,%.3f\n",
                                chain.name.c_str(), legRatio,
                                restLength(result.offsets[thigh]) +
                                    restLength(result.offsets[calf]),
                                restVec.x,
                                restVec.y, restVec.z, r0.x, r0.y, r0.z, r1.x, r1.y,
                                r1.z);
                }
                for (int f = 0; f < source.frames; ++f) {
                    // Hip-relative source delta, converted to target space.
                    const Vector3 relF =
                        Vector3Subtract(wantF[f], hipF[f]);
                    const Vector3 rel0 =
                        Vector3Subtract(wantF[0], hipF[0]);
                    const Vector3 dRaw = Vector3Subtract(relF, rel0);
                    const auto dc = opts.basis.applyPos(
                        {dRaw.x * legRatio, dRaw.y * legRatio, dRaw.z * legRatio},
                        opts.rootScale);
                    // Anchor at current hip: caller adds H each frame below
                    // via stored hip-relative offset.
                    targets.push_back(
                        {restVec.x + dc[0], restVec.y + dc[1], restVec.z + dc[2]});
                }
            }
            std::string ikErr;
            float* errSlot = nullptr;
            float errVal = -1.0f;
            if (report) {
                errSlot = &errVal;
            }
            if (!solveLegIK(result, thigh, calf, foot, targets, ikErr, errSlot)) {
                error = "leg IK (" + chain.name + "): " + ikErr;
                return false;
            }
            if (report) {
                if (chain.name == "LeftLeg") {
                    report->footErrL = errVal;
                } else if (chain.name == "RightLeg") {
                    report->footErrR = errVal;
                }
            }
        }
    }

    out = std::move(result);
    if (report) {
        report->text =
            buildReport(target, map, opts, report->footErrL, report->footErrR);
    }
    return true;
}

std::vector<std::tuple<std::string, std::string, float>>
Retargeter::chainSpanWeights(const SkeletonProfile& target, const ChainDef& chain,
                             const BoneMap& map, const Options& opts) {
    std::vector<std::tuple<std::string, std::string, float>> weights;
    const int T = static_cast<int>(target.joints.size());
    const bool useBind = target.hasBind &&
                         static_cast<int>(target.offsets.size()) == T &&
                         static_cast<int>(target.restLocal.size()) == T;
    ChainParams params;
    const auto pit = opts.chains.find(chain.name);
    if (pit != opts.chains.end()) {
        params = pit->second;
    }
    std::vector<int> order;
    for (const std::string& tn : chain.target) {
        const int ti = findIndex(target.joints, tn);
        if (ti >= 0) {
            order.push_back(ti);
        }
    }
    std::vector<std::string> srcOf(order.size());
    for (size_t k = 0; k < order.size(); ++k) {
        const auto it = map.find(target.joints[order[k]]);
        srcOf[k] = (it != map.end()) ? it->second : std::string();
    }
    size_t k = 0;
    while (k < order.size()) {
        if (srcOf[k].empty() || srcOf[k] == "(none)") {
            weights.emplace_back(target.joints[order[k]], "(none)", 0.0f);
            ++k;
            continue;
        }
        size_t e = k;
        while (e + 1 < order.size() && srcOf[e + 1] == srcOf[k]) {
            ++e;
        }
        float total = 0.0f;
        for (size_t m = k; m <= e; ++m) {
            total += useBind ? restLength(target.offsets[order[m]]) : 1.0f;
        }
        for (size_t m = k; m <= e; ++m) {
            float w = 1.0f;
            if (e > k) {
                const float len =
                    useBind ? restLength(target.offsets[order[m]]) : 1.0f;
                w = (total > 1e-9f) ? len / total : 1.0f / (e - k + 1);
            }
            w *= params.motionScale;
            weights.emplace_back(target.joints[order[m]], srcOf[k], w);
        }
        k = e + 1;
    }
    return weights;
}

std::string Retargeter::buildReport(const SkeletonProfile& target,
                                    const BoneMap& map, const Options& opts,
                                    float footErrL, float footErrR) {
    std::string r;
    char buf[256];
    std::snprintf(buf, sizeof(buf), "profile %s joints=%llu chains=%llu rootScale=%.3f legIK=%d\n",
                  target.id.c_str(),
                  static_cast<unsigned long long>(target.joints.size()),
                  static_cast<unsigned long long>(target.chains.size()),
                  opts.rootScale, opts.legIK ? 1 : 0);
    r += buf;
    std::snprintf(buf, sizeof(buf), "basis rows [%.2f %.2f %.2f] [%.2f %.2f %.2f] [%.2f %.2f %.2f]\n",
                  opts.basis.m[0][0], opts.basis.m[0][1], opts.basis.m[0][2],
                  opts.basis.m[1][0], opts.basis.m[1][1], opts.basis.m[1][2],
                  opts.basis.m[2][0], opts.basis.m[2][1], opts.basis.m[2][2]);
    r += buf;
    for (const ChainDef& chain : target.chains) {
        ChainParams params;
        const auto pit = opts.chains.find(chain.name);
        if (pit != opts.chains.end()) {
            params = pit->second;
        }
        size_t mapped = 0;
        for (const std::string& t : chain.target) {
            const auto it = map.find(t);
            if (it != map.end() && !it->second.empty() && it->second != "(none)") {
                ++mapped;
            }
        }
        std::snprintf(buf, sizeof(buf), "chain %s attached=%llu/%llu enabled=%d scale=%.2f\n",
                      chain.name.c_str(),
                      static_cast<unsigned long long>(mapped),
                      static_cast<unsigned long long>(chain.target.size()),
                      params.enabled ? 1 : 0, params.motionScale);
        r += buf;
        if (params.enabled) {
            for (const auto& [t, s, w] : chainSpanWeights(target, chain, map, opts)) {
                std::snprintf(buf, sizeof(buf), "  %s <- %s w=%.3f\n", t.c_str(),
                              s.c_str(), w);
                r += buf;
            }
        }
    }
    if (opts.legIK) {
        std::snprintf(buf, sizeof(buf), "ik LeftLeg footErr=%.4f RightLeg footErr=%.4f\n",
                      footErrL, footErrR);
    } else {
        std::snprintf(buf, sizeof(buf), "ik disabled\n");
    }
    r += buf;
    const std::vector<std::string> un = unmapped(target, map);
    r += "unmapped:";
    for (const std::string& u : un) {
        r += " " + u;
    }
    r += "\nmissingChains:";
    for (const std::string& m : missingChains(target, map)) {
        r += " " + m;
    }
    r += "\n";
    return r;
}

bool Retargeter::solveLegIK(Animation& anim, int thigh, int calf, int foot,
                            const std::vector<std::array<float, 3>>& footTargets,
                            std::string& error, float* maxErrOut) {
    const int J = anim.joints;
    if (thigh < 0 || calf < 0 || foot < 0 || thigh >= J || calf >= J || foot >= J) {
        error = "bad leg indices";
        return false;
    }
    if (static_cast<int>(footTargets.size()) != anim.frames) {
        error = "foot target count != frames";
        return false;
    }
    // Segment lengths: offset[calf] spans thigh (hip->knee),
    // offset[foot] spans calf (knee->ankle). offsets[thigh] is
    // pelvis->hip and must NOT be used here (off-by-one underestimates
    // reach and over-compresses the leg).
    const float a = restLength(anim.offsets[calf]);
    const float b = restLength(anim.offsets[foot]);
    if (a < 1e-6f || b < 1e-6f) {
        error = "degenerate leg lengths";
        return false;
    }
    auto arcQuat = [](Vector3 from, Vector3 to, Quaternion& outQ) {
        from = Vector3Normalize(from);
        to = Vector3Normalize(to);
        const float d = Vector3DotProduct(from, to);
        if (d > 0.9999f) {
            outQ = {0, 0, 0, 1};
            return;
        }
        Vector3 axis = Vector3CrossProduct(from, to);
        if (Vector3Length(axis) < 1e-6f) {
            axis = Vector3CrossProduct(from, Vector3{1, 0, 0});
            if (Vector3Length(axis) < 1e-6f) {
                axis = Vector3CrossProduct(from, Vector3{0, 1, 0});
            }
            axis = Vector3Normalize(axis);
            outQ = QuaternionFromAxisAngle(axis, 3.14159265f);
            return;
        }
        axis = Vector3Normalize(axis);
        float ang = std::acos(d < -1.0f ? -1.0f : (d > 1.0f ? 1.0f : d));
        outQ = QuaternionFromAxisAngle(axis, ang);
    };
    std::vector<Vector3> pos;
    std::vector<Quaternion> wrot;
    for (int f = 0; f < anim.frames; ++f) {
        float* rots = anim.localRotationsXyzw.data() + static_cast<size_t>(f) * J * 4;
        const float* rp = anim.rootPositions.data() + static_cast<size_t>(f) * 3;
        float root[3] = {rp[0], rp[1], rp[2]};
        Skeleton::forwardKinematicsFull(rots, root, anim.parents, anim.offsets, pos, wrot);
        const Vector3 H = pos[thigh];
        // Early-out: foot already on target (identity/rest clips) — guarantees
        // IK never perturbs a correct pose and skips wasted solves.
        const Vector3 Tt0{H.x + footTargets[f][0], H.y + footTargets[f][1],
                          H.z + footTargets[f][2]};
        if (Vector3Distance(pos[foot], Tt0) < 1e-3f) {
            continue;
        }
        const Vector3 K = pos[calf];
        // Targets are hip-relative offsets (see caller): anchor at this
        // frame's hip so root travel never drags the feet.
        const Vector3 Tt{H.x + footTargets[f][0], H.y + footTargets[f][1],
                         H.z + footTargets[f][2]};
        Vector3 toT = Vector3Subtract(Tt, H);
        float D = Vector3Length(toT);
        const float maxD = a + b - 1e-4f;
        const float minD = std::abs(a - b) + 1e-4f;
        if (D > maxD) {
            D = maxD;
        }
        if (D < minD) {
            D = minD;
        }
        Vector3 u = (Vector3Length(toT) > 1e-9f) ? Vector3Normalize(toT)
                                                 : Vector3{0, -1, 0};
        // Bend plane from current knee (preserves bend direction).
        Vector3 ku = Vector3Subtract(K, H);
        Vector3 lateral = Vector3Subtract(ku, Vector3Scale(u, Vector3DotProduct(ku, u)));
        if (Vector3Length(lateral) < 1e-6f) {
            lateral = Vector3{0, 0, 1};
            Vector3 tmp = Vector3Subtract(lateral,
                                          Vector3Scale(u, Vector3DotProduct(lateral, u)));
            if (Vector3Length(tmp) < 1e-6f) {
                lateral = Vector3{1, 0, 0};
            } else {
                lateral = tmp;
            }
        }
        lateral = Vector3Normalize(lateral);
        float cosA = (a * a + D * D - b * b) / (2 * a * D);
        cosA = cosA < -1.0f ? -1.0f : (cosA > 1.0f ? 1.0f : cosA);
        const float A = std::acos(cosA);
        Vector3 thighDir = Vector3Add(Vector3Scale(u, std::cos(A)),
                                      Vector3Scale(lateral, std::sin(A)));
        thighDir = Vector3Normalize(thighDir);
        Vector3 kneePos = Vector3Add(H, Vector3Scale(thighDir, a));
        Vector3 calfDir = Vector3Subtract(Tt, kneePos);
        calfDir = Vector3Normalize(calfDir);
        // Minimal-arc world quats preserving current twist.
        Vector3 curThighDir = Vector3Normalize(Vector3Subtract(K, H));
        Vector3 footPos = pos[foot];
        Vector3 curCalfDir = Vector3Subtract(footPos, K);
        curCalfDir = Vector3Normalize(curCalfDir);
        Quaternion qThigh, qCalf;
        arcQuat(curThighDir, thighDir, qThigh);
        arcQuat(curCalfDir, calfDir, qCalf);
        Quaternion thighW = QuaternionNormalize(QuaternionMultiply(qThigh, wrot[thigh]));
        Quaternion calfW = QuaternionNormalize(QuaternionMultiply(qCalf, wrot[calf]));
        // Back to parent-relative locals. Parents unaffected (legs are leaves
        // relative to the solved chain; pelvis/ancestors already final).
        const int pThigh = anim.parents[thigh];
        Quaternion thighPW = {0, 0, 0, 1};
        if (pThigh >= 0 && pThigh < J) {
            // Recompute ancestor world from current locals (ancestors final).
            std::vector<Vector3> pp;
            std::vector<Quaternion> pw;
            Skeleton::forwardKinematicsFull(rots, root, anim.parents, anim.offsets, pp,
                                            pw);
            thighPW = pw[pThigh];
        }
        Quaternion thighL =
            QuaternionNormalize(QuaternionMultiply(QuaternionInvert(thighPW), thighW));
        Quaternion calfL =
            QuaternionNormalize(QuaternionMultiply(QuaternionInvert(thighW), calfW));
        rots[thigh * 4] = thighL.x;
        rots[thigh * 4 + 1] = thighL.y;
        rots[thigh * 4 + 2] = thighL.z;
        rots[thigh * 4 + 3] = thighL.w;
        rots[calf * 4] = calfL.x;
        rots[calf * 4 + 1] = calfL.y;
        rots[calf * 4 + 2] = calfL.z;
        rots[calf * 4 + 3] = calfL.w;
    }
    if (maxErrOut) {
        float worst = 0.0f;
        for (int f = 0; f < anim.frames; ++f) {
            const float* rots =
                anim.localRotationsXyzw.data() + static_cast<size_t>(f) * J * 4;
            const float* rp = anim.rootPositions.data() + static_cast<size_t>(f) * 3;
            float root[3] = {rp[0], rp[1], rp[2]};
            Skeleton::forwardKinematicsFull(rots, root, anim.parents, anim.offsets,
                                             pos, wrot);
            const Vector3 H = pos[thigh];
            const Vector3 Tt{H.x + footTargets[f][0], H.y + footTargets[f][1],
                             H.z + footTargets[f][2]};
            worst = std::max(worst, Vector3Distance(pos[foot], Tt));
        }
        *maxErrOut = worst;
    }
    return true;
}

} // namespace studio
