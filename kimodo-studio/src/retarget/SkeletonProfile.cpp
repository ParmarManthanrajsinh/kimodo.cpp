#include "retarget/SkeletonProfile.h"

#include "animation/Skeleton.h"
#include "raymath.h"
#include "retarget/manny_true.inc"

#include <map>

namespace studio {
namespace {

// Core-only filter: fingers, metacarpals, twist bones and end nubs never
// map from SOMA (no source joints) and only bloat tables/exports.
bool keepMannyJoint(const std::string& name) {
    for (const char* drop : {"Thumb", "Index", "Middle", "Ring", "Pinky", "metacarpal",
                             "twist", "HeadTop", "Eye", "Jaw", "_End"}) {
        if (name.find(drop) != std::string::npos) {
            return false;
        }
    }
    return true;
}

int mixamoIndex(const std::string& name) {
    for (int i = 0; i < kMannyTrueJoints; ++i) {
        if (kMannyTrueNames[i] == name) {
            return i;
        }
    }
    return -1;
}

// Build UE5-topology rest pose from measured Mixamo rest data.
// PROVENANCE: topology is UE5 Manny, but transforms come from the
// Mixamo-rigged Manny FBX (Hips/Spine/LeftUpLeg rig, cm Y-up) measured via
// ufbx. This is NOT a native UE5 Manny bind pose. Do not label it true UE5
// until restLocal/offsets are measured from the actual UE5 skeleton
// (SKM_Manny FBX with root/pelvis/spine_01 rig, or uasset export).
// Strategy: Mixamo rest WORLD matrices (via FK over Lcl values) are the
// source of truth. Every UE5 bone takes the world matrix of its Mixamo
// correspondent (or a resampled station), then derives parent-relative
// offset/restLocal against its UE5 parent world. Self-consistent by
// construction; no bind-pose matrices involved.
void buildUe5Manny(SkeletonProfile& ue) {
    // Full Mixamo rest worlds.
    std::vector<float> restFlat(static_cast<size_t>(kMannyTrueJoints) * 4);
    std::vector<int> fullParents(kMannyTrueParents, kMannyTrueParents + kMannyTrueJoints);
    std::vector<std::array<float, 3>> fullOffsets;
    for (int i = 0; i < kMannyTrueJoints; ++i) {
        restFlat[i * 4] = kMannyTrueRest[i][0];
        restFlat[i * 4 + 1] = kMannyTrueRest[i][1];
        restFlat[i * 4 + 2] = kMannyTrueRest[i][2];
        restFlat[i * 4 + 3] = kMannyTrueRest[i][3];
        fullOffsets.push_back({kMannyTrueOffsets[i][0], kMannyTrueOffsets[i][1],
                               kMannyTrueOffsets[i][2]});
    }
    // Seed FK with the Hips Lcl translation: FK places root joints at the
    // given root, so a zero origin would drop the 0.959m Hips height and
    // flatten every derived UE5 offset (pelvis at ground).
    const float origin[3] = {kMannyTrueOffsets[0][0], kMannyTrueOffsets[0][1],
                             kMannyTrueOffsets[0][2]};
    std::vector<Vector3> mixPos;
    std::vector<Quaternion> mixRot;
    Skeleton::forwardKinematicsFull(restFlat.data(), origin, fullParents, fullOffsets,
                                    mixPos, mixRot);
    ue.joints = {"root", "pelvis", "spine_01", "spine_02", "spine_03", "spine_04",
                 "spine_05", "neck_01", "neck_02", "head", "clavicle_l", "upperarm_l",
                 "forearm_l", "hand_l", "clavicle_r", "upperarm_r", "forearm_r",
                 "hand_r", "thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r",
                 "calf_r", "foot_r", "ball_r"};
    ue.parents = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 6, 10, 11, 12, 6,
                  14, 15, 16, 1, 18, 19, 20, 1, 22, 23, 24};
    const int N = 26;
    ue.offsets.assign(N, {0, 0, 0});
    ue.restLocal.assign(N, {0, 0, 0, 1});
    std::vector<Vector3> uePos(N);
    std::vector<Quaternion> ueRot(N);
    ueRot[0] = {0, 0, 0, 1};
    uePos[0] = {0, 0, 0};
    auto placeBone = [&](int ui, const Vector3& wpos, const Quaternion& wrot) {
        uePos[ui] = wpos;
        ueRot[ui] = QuaternionNormalize(wrot);
        const int p = ue.parents[ui];
        Quaternion qi = QuaternionInvert(ueRot[p]);
        Quaternion lq = QuaternionNormalize(QuaternionMultiply(qi, ueRot[ui]));
        Vector3 d = Vector3Subtract(uePos[ui], uePos[p]);
        Vector3 off = Vector3RotateByQuaternion(d, qi);
        ue.offsets[ui] = {off.x, off.y, off.z};
        ue.restLocal[ui] = {lq.x, lq.y, lq.z, lq.w};
    };
    auto mixWorld = [&](const char* mixName, Vector3& pos, Quaternion& rot) {
        const int mi = mixamoIndex(mixName);
        pos = mixPos[mi];
        rot = mixRot[mi];
    };
    // Spine resample stations (absolute) between Hips and Neck worlds.
    std::vector<Vector3> spinePos(7);
    std::vector<Quaternion> spineRot(7);
    {
        const char* spineSrc[6] = {"Hips", "Spine", "Spine1", "Spine2", "Spine3", "Neck"};
        std::vector<Vector3> pts;
        std::vector<Quaternion> rots;
        for (const char* nm : spineSrc) {
            const int mi = mixamoIndex(nm);
            pts.push_back(mixPos[mi]);
            rots.push_back(mixRot[mi]);
        }
        std::vector<Vector3> cumPos;
        std::vector<Quaternion> cumRot;
        // Arc-length resample into 7 stations (0=pelvis anchor, 6=neck anchor).
        std::vector<float> cum(pts.size(), 0.0f);
        for (size_t i = 1; i < pts.size(); ++i) {
            cum[i] = cum[i - 1] + Vector3Distance(pts[i - 1], pts[i]);
        }
        const float total = cum.back();
        for (int k = 0; k < 7; ++k) {
            const float s = (k == 6) ? total : total * static_cast<float>(k) / 6.0f;
            size_t seg = 0;
            while (seg + 1 < pts.size() - 1 && cum[seg + 1] < s) {
                ++seg;
            }
            const float segLen = cum[seg + 1] - cum[seg];
            const float a = (segLen > 1e-9f) ? (s - cum[seg]) / segLen : 0.0f;
            cumPos.push_back(Vector3Lerp(pts[seg], pts[seg + 1], a));
            cumRot.push_back(QuaternionNormalize(QuaternionSlerp(rots[seg], rots[seg + 1], a)));
        }
        for (int k = 0; k < 7; ++k) {
            spinePos[k] = cumPos[k];
            spineRot[k] = cumRot[k];
        }
    }
    Vector3 pos;
    Quaternion rot;
    mixWorld("Hips", pos, rot);
    placeBone(1, pos, rot); // pelvis
    for (int k = 0; k < 5; ++k) {
        placeBone(2 + k, spinePos[1 + k], spineRot[1 + k]); // spine_01..05
    }
    const char* direct[][2] = {
        {"Neck", "neck_01"}, {"Neck1", "neck_02"}, {"Head", "head"},
        {"LeftShoulder", "clavicle_l"}, {"LeftArm", "upperarm_l"},
        {"LeftForeArm", "forearm_l"}, {"LeftHand", "hand_l"},
        {"RightShoulder", "clavicle_r"}, {"RightArm", "upperarm_r"},
        {"RightForeArm", "forearm_r"}, {"RightHand", "hand_r"},
        {"LeftUpLeg", "thigh_l"}, {"LeftLeg", "calf_l"}, {"LeftFoot", "foot_l"},
        {"LeftToeBase", "ball_l"}, {"RightUpLeg", "thigh_r"}, {"RightLeg", "calf_r"},
        {"RightFoot", "foot_r"}, {"RightToeBase", "ball_r"},
    };
    for (const auto& pair : direct) {
        mixWorld(pair[0], pos, rot);
        int ui = -1;
        for (int k = 0; k < N; ++k) {
            if (ue.joints[k] == pair[1]) {
                ui = k;
            }
        }
        if (ui > 0) {
            placeBone(ui, pos, rot);
        }
    }
}

} // namespace

const std::vector<SkeletonProfile>& targetProfiles() {
    static const std::vector<SkeletonProfile> profiles = [] {
        std::vector<SkeletonProfile> out;

        // Mixamo-rig rest pose extracted from user FBX Lcl values
        // (see manny_true.inc header). Units meters. Reduced to the core
        // body: rest offsets/locals recomputed relative to the nearest kept
        // ancestor so dropped mid-chain bones change nothing.
        // NOTE: this is a Mixamo-style rig, not the UE5 Manny skeleton
        // (see the ue5-manny profile below).
        SkeletonProfile manny;
        manny.id = "mixamo-humanoid";
        manny.name = "Mixamo Humanoid";
        manny.hasBind = true;
        std::vector<int> kept;
        for (int i = 0; i < kMannyTrueJoints; ++i) {
            if (keepMannyJoint(kMannyTrueNames[i])) {
                kept.push_back(i);
            }
        }
        // Rest world orientations over the FULL rig (self-consistent).
        std::vector<float> restFlat(static_cast<size_t>(kMannyTrueJoints) * 4);
        std::vector<int> fullParents(kMannyTrueParents, kMannyTrueParents + kMannyTrueJoints);
        std::vector<std::array<float, 3>> fullOffsets;
        for (int i = 0; i < kMannyTrueJoints; ++i) {
            restFlat[i * 4] = kMannyTrueRest[i][0];
            restFlat[i * 4 + 1] = kMannyTrueRest[i][1];
            restFlat[i * 4 + 2] = kMannyTrueRest[i][2];
            restFlat[i * 4 + 3] = kMannyTrueRest[i][3];
            fullOffsets.push_back({kMannyTrueOffsets[i][0], kMannyTrueOffsets[i][1],
                                   kMannyTrueOffsets[i][2]});
        }
        const float origin[3] = {0, 0, 0};
        std::vector<Vector3> restPos;
        std::vector<Quaternion> restWorld;
        Skeleton::forwardKinematicsFull(restFlat.data(), origin, fullParents, fullOffsets,
                                        restPos, restWorld);
        std::map<int, int> newIndex;
        for (size_t k = 0; k < kept.size(); ++k) {
            newIndex[kept[k]] = static_cast<int>(k);
        }
        for (size_t k = 0; k < kept.size(); ++k) {
            const int i = kept[k];
            // Nearest kept ancestor (or -1).
            int anc = kMannyTrueParents[i];
            while (anc >= 0 && newIndex.find(anc) == newIndex.end()) {
                anc = kMannyTrueParents[anc];
            }
            const int newParent = (anc >= 0) ? newIndex[anc] : -1;
            manny.joints.emplace_back(kMannyTrueNames[i]);
            manny.parents.push_back(newParent);
            if (newParent < 0) {
                manny.offsets.push_back(fullOffsets[i]);
                manny.restLocal.push_back({kMannyTrueRest[i][0], kMannyTrueRest[i][1],
                                           kMannyTrueRest[i][2], kMannyTrueRest[i][3]});
            } else {
                const int a = kept[newParent];
                Quaternion qi = QuaternionInvert(restWorld[a]);
                Quaternion lq = QuaternionNormalize(QuaternionMultiply(qi, restWorld[i]));
                Vector3 d = Vector3Subtract(restPos[i], restPos[a]);
                Vector3 off = Vector3RotateByQuaternion(d, qi);
                manny.offsets.push_back({off.x, off.y, off.z});
                manny.restLocal.push_back({lq.x, lq.y, lq.z, lq.w});
            }
        }
        manny.defaultMap = {
            {"Hips", "Hips"},
            {"Spine", "Spine1"},
            {"Spine1", "Spine2"},
            {"Spine2", "Chest"},
            {"Spine3", "Chest"},
            {"Neck", "Neck1"},
            {"Neck1", "Neck2"},
            {"Head", "Head"},
            {"LeftShoulder", "LeftShoulder"},
            {"LeftArm", "LeftArm"},
            {"LeftForeArm", "LeftForeArm"},
            {"LeftHand", "LeftHand"},
            {"RightShoulder", "RightShoulder"},
            {"RightArm", "RightArm"},
            {"RightForeArm", "RightForeArm"},
            {"RightHand", "RightHand"},
            {"LeftUpLeg", "LeftLeg"},
            {"LeftLeg", "LeftShin"},
            {"LeftFoot", "LeftFoot"},
            {"LeftToeBase", "LeftToeBase"},
            {"RightUpLeg", "RightLeg"},
            {"RightLeg", "RightShin"},
            {"RightFoot", "RightFoot"},
            {"RightToeBase", "RightToeBase"},
        };
        manny.chains = {
            {"Pelvis", {"Hips"}, {"Hips"}},
            {"Spine", {"Spine", "Spine1", "Spine2", "Spine3"}, {"Spine1", "Spine2", "Chest"}},
            {"Neck", {"Neck", "Neck1"}, {"Neck1", "Neck2"}},
            {"Head", {"Head"}, {"Head"}},
            {"LeftArm", {"LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand"},
             {"LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand"}},
            {"RightArm", {"RightShoulder", "RightArm", "RightForeArm", "RightHand"},
             {"RightShoulder", "RightArm", "RightForeArm", "RightHand"}},
            {"LeftLeg", {"LeftUpLeg", "LeftLeg", "LeftFoot", "LeftToeBase"},
             {"LeftLeg", "LeftShin", "LeftFoot", "LeftToeBase"}},
            {"RightLeg", {"RightUpLeg", "RightLeg", "RightFoot", "RightToeBase"},
             {"RightLeg", "RightShin", "RightFoot", "RightToeBase"}},
        };
        out.push_back(std::move(manny));

        // UE5 Manny topology (root/pelvis/spine_01..05, neck/head,
        // full limb chains). Rest shape resampled from the Mixamo rest data
        // above (same ~180cm humanoid); see provenance note in builder.
        {
            SkeletonProfile ue;
            ue.id = "unreal-manny";
            ue.name = "UE5 Manny";
            ue.hasBind = true;
            ue.bindProvenance = "mixamo-measured-topology-ue5";
            buildUe5Manny(ue);
            ue.defaultMap = {
                {"pelvis", "Hips"},
                {"spine_01", "Spine1"}, {"spine_02", "Spine1"},
                {"spine_03", "Spine2"}, {"spine_04", "Chest"}, {"spine_05", "Chest"},
                {"neck_01", "Neck1"}, {"neck_02", "Neck2"}, {"head", "Head"},
                {"clavicle_l", "LeftShoulder"}, {"upperarm_l", "LeftArm"},
                {"forearm_l", "LeftForeArm"}, {"hand_l", "LeftHand"},
                {"clavicle_r", "RightShoulder"}, {"upperarm_r", "RightArm"},
                {"forearm_r", "RightForeArm"}, {"hand_r", "RightHand"},
                {"thigh_l", "LeftLeg"}, {"calf_l", "LeftShin"},
                {"foot_l", "LeftFoot"}, {"ball_l", "LeftToeBase"},
                {"thigh_r", "RightLeg"}, {"calf_r", "RightShin"},
                {"foot_r", "RightFoot"}, {"ball_r", "RightToeBase"},
            };
            ue.chains = {
                {"Pelvis", {"pelvis"}, {"Hips"}},
                {"Spine", {"spine_01", "spine_02", "spine_03", "spine_04", "spine_05"},
                 {"Spine1", "Spine2", "Chest"}},
                {"Neck", {"neck_01", "neck_02"}, {"Neck1", "Neck2"}},
                {"Head", {"head"}, {"Head"}},
                {"LeftArm", {"clavicle_l", "upperarm_l", "forearm_l", "hand_l"},
                 {"LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand"}},
                {"RightArm", {"clavicle_r", "upperarm_r", "forearm_r", "hand_r"},
                 {"RightShoulder", "RightArm", "RightForeArm", "RightHand"}},
                {"LeftLeg", {"thigh_l", "calf_l", "foot_l", "ball_l"},
                 {"LeftLeg", "LeftShin", "LeftFoot", "LeftToeBase"}},
                {"RightLeg", {"thigh_r", "calf_r", "foot_r", "ball_r"},
                 {"RightLeg", "RightShin", "RightFoot", "RightToeBase"}},
            };
            out.push_back(std::move(ue));
        }

        SkeletonProfile humanoid;
        humanoid.id = "unity-humanoid";
        humanoid.name = "Unity Humanoid";
        humanoid.joints = {"Hips",          "Spine",         "Chest",
                           "Neck",          "Head",          "LeftShoulder",
                           "LeftUpperArm",  "LeftLowerArm",  "LeftHand",
                           "RightShoulder", "RightUpperArm", "RightLowerArm",
                           "RightHand",     "LeftUpperLeg",  "LeftLowerLeg",
                           "LeftFoot",      "LeftToes",      "RightUpperLeg",
                           "RightLowerLeg", "RightFoot",     "RightToes"};
        humanoid.parents = {-1, 0, 1, 2, 3, 2, 5, 6, 7, 2, 9, 10, 11,
                            0, 13, 14, 15, 0, 17, 18, 19};
        humanoid.defaultMap = {
            {"Hips", "Hips"},           {"Spine", "Spine1"},
            {"Chest", "Spine2"},        {"Neck", "Neck1"},
            {"Head", "Head"},           {"LeftShoulder", "LeftShoulder"},
            {"LeftUpperArm", "LeftArm"}, {"LeftLowerArm", "LeftForeArm"},
            {"LeftHand", "LeftHand"},   {"RightShoulder", "RightShoulder"},
            {"RightUpperArm", "RightArm"}, {"RightLowerArm", "RightForeArm"},
            {"RightHand", "RightHand"}, {"LeftUpperLeg", "LeftLeg"},
            {"LeftLowerLeg", "LeftShin"}, {"LeftFoot", "LeftFoot"},
            {"LeftToes", "LeftToeBase"}, {"RightUpperLeg", "RightLeg"},
            {"RightLowerLeg", "RightShin"}, {"RightFoot", "RightFoot"},
            {"RightToes", "RightToeBase"},
        };
        out.push_back(std::move(humanoid));

        SkeletonProfile blender;
        blender.id = "blender-generic";
        blender.name = "Blender (generic)";
        blender.joints = {"Hips", "Spine1", "Spine2", "Chest", "Neck1", "Neck2",
                          "Head", "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand",
                          "RightShoulder", "RightArm", "RightForeArm", "RightHand",
                          "LeftLeg", "LeftShin", "LeftFoot", "LeftToeBase",
                          "RightLeg", "RightShin", "RightFoot", "RightToeBase"};
        blender.parents = {-1, 0, 1, 2, 3, 4, 5, 3, 7, 8, 9, 3,
                           11, 12, 13, 0, 15, 16, 17, 0, 19, 20, 21};
        for (const std::string& j : blender.joints) {
            blender.defaultMap.emplace_back(j, j);
        }
        out.push_back(std::move(blender));

        return out;
    }();
    return profiles;
}

// Expected UE5 Manny core topology (STEP 2). Fingers, toes, twist bones
// excluded by design (no SOMA source joints); everything else must match
// exactly or the profile is not UE5-Manny-compatible.
static const char* kUe5CoreJoints[] = {
    "root", "pelvis", "spine_01", "spine_02", "spine_03", "spine_04",
    "spine_05", "neck_01", "neck_02", "head", "clavicle_l", "upperarm_l",
    "forearm_l", "hand_l", "clavicle_r", "upperarm_r", "forearm_r",
    "hand_r", "thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r",
    "calf_r", "foot_r", "ball_r",
};
static const int kUe5CoreParents[] = {-1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 6, 10,
                                      11, 12, 6, 14, 15, 16, 1, 18, 19, 20,
                                      1, 22, 23, 24};

bool buildTargetReference(const SkeletonProfile& profile, TargetReference& out,
                          std::string& error) {
    const int T = static_cast<int>(profile.joints.size());
    const int N = static_cast<int>(sizeof(kUe5CoreJoints) / sizeof(kUe5CoreJoints[0]));
    if (T != N) {
        error = "UE5 hierarchy: joint count " + std::to_string(T) + " != " +
                std::to_string(N);
        return false;
    }
    for (int i = 0; i < N; ++i) {
        if (profile.joints[i] != kUe5CoreJoints[i] || profile.parents[i] != kUe5CoreParents[i]) {
            error = std::string("UE5 hierarchy mismatch at ") + std::to_string(i);
            return false;
        }
    }
    if (!profile.hasBind || static_cast<int>(profile.offsets.size()) != T ||
        static_cast<int>(profile.restLocal.size()) != T) {
        error = "UE5 reference: incomplete bind data";
        return false;
    }
    std::vector<float> restFlat(static_cast<size_t>(T) * 4);
    for (int t = 0; t < T; ++t) {
        restFlat[t * 4] = profile.restLocal[t][0];
        restFlat[t * 4 + 1] = profile.restLocal[t][1];
        restFlat[t * 4 + 2] = profile.restLocal[t][2];
        restFlat[t * 4 + 3] = profile.restLocal[t][3];
    }
    const float origin[3] = {0, 0, 0};
    out.profile = &profile;
    std::vector<Vector3> pos;
    std::vector<Quaternion> rot;
    Skeleton::forwardKinematicsFull(restFlat.data(), origin, profile.parents,
                                    profile.offsets, pos, rot);
    out.valid = static_cast<int>(pos.size()) == T &&
                static_cast<int>(rot.size()) == T;
    if (!out.valid) {
        error = "UE5 reference: FK failed";
        return false;
    }
    out.worldPos.assign(T, {0, 0, 0});
    out.worldRot.assign(T, {0, 0, 0, 1});
    for (int t = 0; t < T; ++t) {
        out.worldPos[t] = {pos[t].x, pos[t].y, pos[t].z};
        out.worldRot[t] = {rot[t].x, rot[t].y, rot[t].z, rot[t].w};
    }
    return true;
}

const SkeletonProfile* findProfile(const std::string& id) {
    for (const SkeletonProfile& p : targetProfiles()) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

} // namespace studio
