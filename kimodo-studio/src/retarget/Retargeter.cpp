#include "retarget/Retargeter.h"

#include "animation/Skeleton.h"
#include "raymath.h"
#include "retarget/SkeletonProfile.h"

#include <cmath>

namespace studio {

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
        const auto it = map.find(j);
        if (it == map.end() || it->second.empty() || it->second == "(none)") {
            out.push_back(j);
        }
    }
    return out;
}

bool Retargeter::retarget(const Animation& source, const SkeletonProfile& target,
                          const BoneMap& map, const Options& opts, Animation& out,
                          std::string& error) {
    if (source.empty()) {
        error = "source animation empty";
        return false;
    }
    // Source joint index by name (SOMA topology).
    auto srcIndex = [&source](const std::string& name) -> int {
        for (size_t i = 0; i < source.jointNames.size(); ++i) {
            if (source.jointNames[i] == name) {
                return static_cast<int>(i);
            }
        }
        return -1;
    };

    const int T = static_cast<int>(target.joints.size());
    const int S = source.joints;
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

    // Resolve map to indices once.
    std::vector<int> tgtToSrc(T, -1);
    for (int t = 0; t < T; ++t) {
        const auto it = map.find(target.joints[t]);
        if (it != map.end() && !it->second.empty() && it->second != "(none)") {
            tgtToSrc[t] = srcIndex(it->second);
        }
    }

    // Rest offsets: from target bind when available (true proportions),
    // else transferred through the map (legacy profiles).
    if (target.hasBind && static_cast<int>(target.offsets.size()) == T) {
        result.offsets = target.offsets;
    } else {
        for (int t = 0; t < T; ++t) {
            const int s = tgtToSrc[t];
            if (s >= 0 && static_cast<size_t>(s) < source.offsets.size()) {
                result.offsets[t] = source.offsets[s];
            }
        }
    }

    // Rest-offset rotation transfer (upstream Kimodo convention):
    //   tgtWorld = srcWorld * (inv(srcRest) * tgtRest), source rest = identity
    //   tgtLocal = inv(parentAnimatedWorld) * tgtWorld
    // Rest worlds use yaw-only root normalization (keeps upright stance and
    // source facing; full-orientation normalize tips, none about-faces).
    // The mapped root keeps the source world orientation exactly so global
    // facing/travel match the source clip. At source
    // rest this reproduces the normalized target rest; during motion it
    // carries relative articulation with source facing. Unmapped hold rest.
    const bool useBind = target.hasBind &&
                         static_cast<int>(target.restLocal.size()) == T;
    std::vector<Quaternion> tgtRestWorld(T, {0, 0, 0, 1});
    if (useBind) {
        std::vector<float> restFlat(static_cast<size_t>(T) * 4);
        for (int t = 0; t < T; ++t) {
            restFlat[t * 4] = target.restLocal[t][0];
            restFlat[t * 4 + 1] = target.restLocal[t][1];
            restFlat[t * 4 + 2] = target.restLocal[t][2];
            restFlat[t * 4 + 3] = target.restLocal[t][3];
        }
        const float origin[3] = {0, 0, 0};
        std::vector<Vector3> dummy;
        Skeleton::forwardKinematicsFull(restFlat.data(), origin, target.parents,
                                        result.offsets, dummy, tgtRestWorld);
        // Yaw-only root normalization: align facing with the source without
        // tipping (a full-orientation normalize tips the figure; none leaves
        // a constant about-face when the rig rest root carries yaw).
        int rootIdx = 0;
        for (int t = 0; t < T; ++t) {
            if (target.parents[t] < 0) {
                rootIdx = t;
                break;
            }
        }
        {
            const Quaternion r = tgtRestWorld[rootIdx];
            const float siny = 2.0f * (r.w * r.y + r.x * r.z);
            const float cosy = 1.0f - 2.0f * (r.y * r.y + r.z * r.z);
            const float half = -0.5f * std::atan2(siny, cosy);
            const Quaternion yawOnly{0.0f, std::sin(half), 0.0f, std::cos(half)};
            for (int t = 0; t < T; ++t) {
                tgtRestWorld[t] = QuaternionNormalize(
                    QuaternionMultiply(yawOnly, tgtRestWorld[t]));
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
        if (useBind) {
            Skeleton::forwardKinematicsFull(srcRots, srcRoot, source.parents,
                                            source.offsets, srcPos, srcWorld);
            // Topological order: parents precede children in both profiles.
            for (int t = 0; t < T; ++t) {
                const int s = tgtToSrc[t];
                float* q = dstRots + t * 4;
                Quaternion tw;
                const bool isRoot = target.parents[t] < 0;
                if (s >= 0 && s < S && s < static_cast<int>(srcWorld.size())) {
                    // Mapped root keeps source world: exact facing/travel.
                    // Others compose source world with yaw-normalized rest.
                    tw = isRoot ? QuaternionNormalize(srcWorld[s])
                                : QuaternionNormalize(QuaternionMultiply(
                                      srcWorld[s], tgtRestWorld[t]));
                } else {
                    // Unmapped: rest world (holds relaxed rest pose).
                    tw = tgtRestWorld[t];
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
        } else {
            for (int t = 0; t < T; ++t) {
                const int s = tgtToSrc[t];
                float* q = dstRots + t * 4;
                if (s >= 0) {
                    q[0] = srcRots[s * 4];
                    q[1] = srcRots[s * 4 + 1];
                    q[2] = srcRots[s * 4 + 2];
                    q[3] = srcRots[s * 4 + 3];
                } else {
                    q[0] = 0;
                    q[1] = 0;
                    q[2] = 0;
                    q[3] = 1; // identity bind
                }
            }
        }
        float* dstRoot = result.rootPositions.data() + static_cast<size_t>(f) * 3;
        dstRoot[0] = srcRoot[0] * opts.rootScale;
        dstRoot[1] = srcRoot[1] * opts.rootScale;
        dstRoot[2] = srcRoot[2] * opts.rootScale;
    }

    out = std::move(result);
    return true;
}

} // namespace studio
