#include "retarget/Retargeter.h"

#include <sstream>

namespace studio {
namespace {

int find_index(const std::vector<std::string>& names, const std::string& name) {
    for (size_t i = 0; i < names.size(); ++i) {
        if (names[i] == name)
            return static_cast<int>(i);
    }
    return -1;
}

std::string build_report(const SkeletonProfile& target, const BoneMap& map, int mapped, int unmapped) {
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

BoneMap Retargeter::AutoMap(const SkeletonProfile& profile) {
    BoneMap map;
    for (const auto& [tgt, src] : profile.default_map) {
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

bool Retargeter::retarget(const Animation& source, const SkeletonProfile& target, const BoneMap& map,
                          const Options& opts, Animation& out, std::string& error, RetargetReport* report) {
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

    auto src_index = [&source](const std::string& name) -> int { return find_index(source.joint_names, name); };

    Animation result;
    result.frames = source.frames;
    result.joints = T;
    result.fps = source.fps;
    result.skeleton_name = target.id;
    result.joint_names = target.joints;
    result.parents = target.parents;
    result.offsets.resize(T, {0, 0, 0});
    result.local_rotations_xyzw.assign(static_cast<size_t>(source.frames) * T * 4, 0.0f);
    result.root_positions.resize(static_cast<size_t>(source.frames) * 3);

    std::vector<int> tgt_to_src(T, -1);
    int mapped_count = 0;
    int unmapped_count = 0;

    for (int t = 0; t < T; ++t) {
        const auto it = map.find(target.joints[t]);
        if (it != map.end() && !it->second.empty() && it->second != "(none)") {
            tgt_to_src[t] = src_index(it->second);
            if (tgt_to_src[t] >= 0) {
                mapped_count++;
            } else {
                unmapped_count++;
            }
        } else {
            unmapped_count++;
        }
    }

    // Transfer offsets from mapped source joints if available
    for (int t = 0; t < T; ++t) {
        const int s = tgt_to_src[t];
        if (s >= 0 && static_cast<size_t>(s) < source.offsets.size()) {
            result.offsets[t] = source.offsets[s];
        } else if (t < static_cast<int>(target.offsets.size())) {
            result.offsets[t] = target.offsets[t];
        }
    }

    for (int f = 0; f < source.frames; ++f) {
        const float* src_rots = source.local_rotations_xyzw.data() + static_cast<size_t>(f) * S * 4;
        const float* src_root = source.root_positions.data() + static_cast<size_t>(f) * 3;
        float* dst_rots = result.local_rotations_xyzw.data() + static_cast<size_t>(f) * T * 4;

        for (int t = 0; t < T; ++t) {
            const int s = tgt_to_src[t];
            float* q = dst_rots + t * 4;
            if (s >= 0) {
                q[0] = src_rots[s * 4 + 0];
                q[1] = src_rots[s * 4 + 1];
                q[2] = src_rots[s * 4 + 2];
                q[3] = src_rots[s * 4 + 3];
            } else {
                // Identity rotation for unmapped joints
                q[0] = 0.0f;
                q[1] = 0.0f;
                q[2] = 0.0f;
                q[3] = 1.0f;
            }
        }

        float* dst_root = result.root_positions.data() + static_cast<size_t>(f) * 3;
        dst_root[0] = src_root[0] * opts.root_scale;
        dst_root[1] = src_root[1] * opts.root_scale;
        dst_root[2] = src_root[2] * opts.root_scale;
    }

    out = std::move(result);
    if (report) {
        report->mapped_count = mapped_count;
        report->unmapped_count = unmapped_count;
        report->text = build_report(target, map, mapped_count, unmapped_count);
    }
    return true;
}

} // namespace studio
