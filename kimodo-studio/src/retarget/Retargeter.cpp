#include "retarget/Retargeter.h"
#include "animation/Skeleton.h"

#include <cmath>
#include <sstream>

namespace studio {
namespace {

int findIndex(const std::vector<std::string>& names, const std::string& name) {
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name) return static_cast<int>(i);
    }
    return -1;
}

std::string buildReport(const SkeletonProfile& target, const BoneMap& map,
                        int mapped, int unmapped) {
    (void)map;
    std::ostringstream ss;
    ss << "Profile: " << target.name << " (" << target.id << ")\n";
    ss << "Mapped joints: " << mapped << " / " << target.joints.size() << "\n";
    if (unmapped > 0) {
        ss << "Unmapped joints: " << unmapped << " (holding identity)\n";
    }
    return ss.str();
}

} // namespace

BoneMap Retargeter::autoMap(const SkeletonProfile& profile) {
    BoneMap map;
    for (const auto& [tgt, src] : profile.defaultMap) {
        map[tgt] = src;
    }
    return map;
}

std::vector<std::string> Retargeter::unmapped(const SkeletonProfile& profile, const BoneMap& map) {
    std::vector<std::string> out;
    for (const auto& j : profile.joints) {
        const auto it = map.find(j);
        if (it == map.end() || it->second.empty() || it->second == "(none)") {
            out.push_back(j);
        }
    }
    return out;
}

bool Retargeter::retarget(const Animation& source, const SkeletonProfile& target,
                         const BoneMap& map, const Options& opts, Animation& out,
                         std::string& error, RetargetReport* report) {
    if (source.empty()) {
        error = "Source animation is empty";
        return false;
    }
    const int T = static_cast<int>(target.joints.size());
    const int S = source.joints;
    if (T <= 0 || S <= 0) {
        error = "Invalid target or source topology";
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

    std::vector<int> tgtToSrc(T, -1);
    int mappedCount = 0;
    int unmappedCount = 0;

    for (int t = 0; t < T; ++t) {
        const auto it = map.find(target.joints[t]);
        if (it != map.end() && !it->second.empty() && it->second != "(none)") {
            tgtToSrc[t] = srcIndex(it->second);
            if (tgtToSrc[t] >= 0) {
                mappedCount++;
            } else {
                unmappedCount++;
            }
        } else {
            unmappedCount++;
        }
    }

    // Transfer offsets from mapped source joints if available
    for (int t = 0; t < T; ++t) {
        const int s = tgtToSrc[t];
        if (s >= 0 && static_cast<size_t>(s) < source.offsets.size()) {
            result.offsets[t] = source.offsets[s];
        } else if (t < static_cast<int>(target.offsets.size())) {
            result.offsets[t] = target.offsets[t];
        }
    }

    for (int f = 0; f < source.frames; ++f) {
        const float* srcRots = source.localRotationsXyzw.data() + static_cast<size_t>(f) * S * 4;
        const float* srcRoot = source.rootPositions.data() + static_cast<size_t>(f) * 3;
        float* dstRots = result.localRotationsXyzw.data() + static_cast<size_t>(f) * T * 4;

        for (int t = 0; t < T; ++t) {
            const int s = tgtToSrc[t];
            float* q = dstRots + t * 4;
            if (s >= 0) {
                q[0] = srcRots[s * 4 + 0];
                q[1] = srcRots[s * 4 + 1];
                q[2] = srcRots[s * 4 + 2];
                q[3] = srcRots[s * 4 + 3];
            } else {
                // Identity rotation for unmapped joints
                q[0] = 0.0f;
                q[1] = 0.0f;
                q[2] = 0.0f;
                q[3] = 1.0f;
            }
        }

        float* dstRoot = result.rootPositions.data() + static_cast<size_t>(f) * 3;
        dstRoot[0] = srcRoot[0] * opts.rootScale;
        dstRoot[1] = srcRoot[1] * opts.rootScale;
        dstRoot[2] = srcRoot[2] * opts.rootScale;
    }

    out = std::move(result);
    if (report) {
        report->mappedCount = mappedCount;
        report->unmappedCount = unmappedCount;
        report->text = buildReport(target, map, mappedCount, unmappedCount);
    }
    return true;
}

} // namespace studio
