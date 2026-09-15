#pragma once

#include <array>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

#include "animation/Animation.h"

namespace studio {

struct SkeletonProfile;
struct ChainDef;

// Filled by retarget() when requested: IK foot residuals + summary text.
struct RetargetReport {
    float footErrL = -1.0f; // -1 = IK did not run for this leg
    float footErrR = -1.0f;
    std::string text;
};

// target joint -> source joint ("(none)" = identity bind).
using BoneMap = std::map<std::string, std::string>;

// Named, testable source->target frame conversion (rotation only;
// translations scale separately via Options). Identity when both
// skeletons share a frame (SOMA -> Mixamo/UE5, meters Y-up).
struct BasisConvert {
    float m[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    static BasisConvert identity() { return BasisConvert{}; }
    static BasisConvert yUpToZUp(); // test helper: (x,y,z) -> (x,z,-y)
    std::array<float, 3> applyPos(const std::array<float, 3>& v, float scale) const;
    std::array<float, 4> applyQuat(const std::array<float, 4>& q) const; // xyzw
};

// Per-chain calibration, exposed in UI separately from bone mapping.
struct ChainParams {
    bool enabled = true;
    float motionScale = 1.0f; // 0 = hold rest, 1 = full motion
};

class Retargeter {
public:
    // Default alias map for a profile.
    static BoneMap autoMap(const SkeletonProfile& target);

    // Unmapped target joints (need manual mapping). Root ("root") never maps.
    static std::vector<std::string> unmapped(const SkeletonProfile& target,
                                             const BoneMap& map);

    // Chains with zero mapped bones. Non-empty = caller must report/fail.
    static std::vector<std::string> missingChains(const SkeletonProfile& target,
                                                  const BoneMap& map);

    struct Options {
        float rootScale = 1.0f; // root translation scale into target units
        BasisConvert basis;     // source -> target frame conversion
        bool legIK = true;      // two-bone leg IK post-pass (feet keep contact)
        std::map<std::string, ChainParams> chains; // override per chain name
    };

    // Chain-based rest-offset retarget. Fails naming the missing chains.
    // report (optional) receives foot errors + a printable summary.
    static bool retarget(const Animation& source, const SkeletonProfile& target,
                         const BoneMap& map, const Options& opts, Animation& out,
                         std::string& error, RetargetReport* report = nullptr);

    // Analytic two-bone IK for one leg, in place on anim frames.
    // Thigh/calf/foot indices refer to anim topology. footTargets are
    // hip-relative offsets. maxErrOut (optional) receives the worst
    // post-solve |foot - target| over frames.
    static bool solveLegIK(Animation& anim, int thigh, int calf, int foot,
                           const std::vector<std::array<float, 3>>& footTargets,
                           std::string& error, float* maxErrOut = nullptr);

    // Printable req-15 summary: chains/weights/IK errors/basis/missing.
    static std::string buildReport(const SkeletonProfile& target,
                                   const BoneMap& map, const Options& opts,
                                   float footErrL, float footErrR);

    // Per-target applied span weights: (target, source, cumulative W).
    // Cumulative across each shared-source group (last member = full
    // delta) so series composition preserves total motion.
    static std::vector<std::tuple<std::string, std::string, float>>
    chainSpanWeights(const SkeletonProfile& target, const ChainDef& chain,
                     const BoneMap& map, const Options& opts);
};

} // namespace studio
