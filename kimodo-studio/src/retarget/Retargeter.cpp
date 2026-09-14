#include "retarget/Retargeter.h"

#include "retarget/SkeletonProfile.h"

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
        const int s = tgtToSrc[t];
        if (s >= 0 && static_cast<size_t>(s) < source.offsets.size()) {
            result.offsets[t] = source.offsets[s];
        }
    }

    for (int f = 0; f < source.frames; ++f) {
        const float* srcRots =
            source.localRotationsXyzw.data() + static_cast<size_t>(f) * source.joints * 4;
        float* dstRots =
            result.localRotationsXyzw.data() + static_cast<size_t>(f) * T * 4;
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
        const float* srcRoot =
            source.rootPositions.data() + static_cast<size_t>(f) * 3;
        float* dstRoot = result.rootPositions.data() + static_cast<size_t>(f) * 3;
        dstRoot[0] = srcRoot[0] * opts.rootScale;
        dstRoot[1] = srcRoot[1] * opts.rootScale;
        dstRoot[2] = srcRoot[2] * opts.rootScale;
    }

    out = std::move(result);
    return true;
}

} // namespace studio
