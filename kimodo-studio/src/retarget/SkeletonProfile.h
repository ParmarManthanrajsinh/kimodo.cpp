#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace studio {

// Target skeleton topology. Profiles with hasBind carry their own rest pose
// (offsets + rest-local quats, e.g. extracted from the real DCC skeleton);
// others transfer source offsets through the joint map (rotation retarget,
// proportions preserved).
//
// Retarget chains group ordered target joints with ordered source joints.
// A chain with equal counts transfers 1:1; a longer target span splits the
// shared source motion by rest-length weights (never duplicates a full
// source rotation across several targets).
struct ChainDef {
    std::string name;                 // "LeftLeg"
    std::vector<std::string> target;  // ordered root -> tip (profile names)
    std::vector<std::string> source;  // ordered root -> tip (SOMA names)
};

// Explicit profile capability (Phase 2). Never inferred from incidental
// fields (hasBind, chains non-empty, joint names).
enum class RetargetMode {
    // Direct local copy: out local = source local, offsets from mapped
    // source, unmapped hold identity. No bind data, no basis conversion,
    // no spans, no IK. blender-generic, unity-humanoid. Stable path.
    // Blender regression cause (fixed): ChainReferencePose math leaked
    // into GenericLocal via shared parent-world derivation. Fix: early
    // return byte-exact local copy, no UE bind/span/IK/root corrections.
    GenericLocal,
    // DEPRECATED: Reference-pose chains for unreal-manny only. Kept for
    // backward compat. New code: export BVH Humanoid, retarget in UE
    // IK Retargeter. Do not extend.
    ChainReferencePose,
};

struct SkeletonProfile {
    std::string id;   // "ue5-manny"
    std::string name; // "UE5 Manny"
    RetargetMode mode = RetargetMode::GenericLocal;
    std::vector<std::string> joints;
    std::vector<int> parents;
    // Default source joint per target joint (alias table, explicit).
    std::vector<std::pair<std::string, std::string>> defaultMap;
    bool hasBind = false;
    std::vector<std::array<float, 3>> offsets; // rest offsets (meters)
    std::vector<std::array<float, 4>> restLocal; // rest-local quats xyzw
    std::vector<ChainDef> chains; // ChainReferencePose only
    // Bind provenance: where restLocal/offsets actually came from.
    // "ue5-native" = measured from UE5 Manny skeleton. Anything else is
    // NOT a true UE5 bind pose, even if the topology is UE5-style.
    std::string bindProvenance;
};

// Clean target reference representation (STEP 1): local bind data plus
// derived world bind pose, built once from a profile. Names/parents are
// views into the profile; worlds are computed by FK and cached here so
// retarget math and tests share one reference, never recompute ad-hoc.
struct TargetReference {
    const SkeletonProfile* profile = nullptr;
    std::vector<std::array<float, 3>> worldPos; // world bind positions (meters)
    std::vector<std::array<float, 4>> worldRot; // world bind quats xyzw
    bool valid = false;
};

// Source (SOMA/Kimodo) reference convention (STEP 6): Kimodo core emits
// absolute local rotations; the reference pose is identity local quats
// over Soma30Spec offsets. No baked rest rotations. Asserted at retarget
// entry by convention, not per-clip data (core provides no rest pose).
struct SourceReference {
    static constexpr float kTol = 1e-4f;
};

// Build + validate the target reference. Fails (returns false) unless the
// hierarchy matches the expected UE5 Manny core topology exactly.
bool buildTargetReference(const SkeletonProfile& profile, TargetReference& out,
                          std::string& error);

const std::vector<SkeletonProfile>& targetProfiles();
const SkeletonProfile* findProfile(const std::string& id);

} // namespace studio
