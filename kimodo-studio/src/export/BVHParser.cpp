#include "export/BVHParser.h"
#include "raymath.h"

#include <cmath>
#include <fstream>
#include <sstream>

namespace studio {

void BVHParser::EulerXyzToQuat(float ex, float ey, float ez, float& x, float& y, float& z, float& w) {
    constexpr float kRad = 0.017453292519943295f; // pi / 180
    const float hx = ex * kRad * 0.5f;
    const float hy = ey * kRad * 0.5f;
    const float hz = ez * kRad * 0.5f;

    const float sx = std::sin(hx), cx = std::cos(hx);
    const float sy = std::sin(hy), cy = std::cos(hy);
    const float sz = std::sin(hz), cz = std::cos(hz);

    Quaternion qx{sx, 0.0f, 0.0f, cx};
    Quaternion qy{0.0f, sy, 0.0f, cy};
    Quaternion qz{0.0f, 0.0f, sz, cz};

    Quaternion q = QuaternionMultiply(qz, QuaternionMultiply(qy, qx));
    q = QuaternionNormalize(q);

    x = q.x;
    y = q.y;
    z = q.z;
    w = q.w;
}

namespace {

struct TokenStream {
    std::vector<std::string> tokens;
    size_t cursor = 0;

    bool HasNext() const { return cursor < tokens.size(); }
    const std::string& next() { return tokens[cursor++]; }
    const std::string& peek() const { return tokens[cursor]; }
    void backtrack() {
        if (cursor > 0)
            cursor--;
    }
};

TokenStream tokenize(const std::string& text) {
    TokenStream ts;
    std::string cur;
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) {
                ts.tokens.push_back(cur);
                cur.clear();
            }
        } else if (c == '{' || c == '}') {
            if (!cur.empty()) {
                ts.tokens.push_back(cur);
                cur.clear();
            }
            ts.tokens.push_back(std::string(1, c));
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) {
        ts.tokens.push_back(cur);
    }
    return ts;
}

struct ParsedJoint {
    std::string name;
    int parent = -1;
    std::array<float, 3> offset{0, 0, 0};
    int channel_count = 0;
    std::vector<std::string> channel_types;
};

} // namespace

bool BVHParser::ParseString(const std::string& bvh_text, Animation& out_animation, std::string& error) {
    TokenStream ts = tokenize(bvh_text);
    if (!ts.HasNext()) {
        error = "BVH text is empty";
        return false;
    }

    if (ts.next() != "HIERARCHY") {
        error = "Expected 'HIERARCHY' token";
        return false;
    }

    std::vector<ParsedJoint> joints;
    std::vector<int> joint_stack;

    // Parse HIERARCHY section
    while (ts.HasNext()) {
        const std::string tok = ts.next();
        if (tok == "MOTION") {
            break;
        }

        if (tok == "ROOT" || tok == "JOINT") {
            if (!ts.HasNext()) {
                error = "Unexpected end of tokens after " + tok;
                return false;
            }
            std::string jname = ts.next();
            int parent_idx = joint_stack.empty() ? -1 : joint_stack.back();
            int new_idx = static_cast<int>(joints.size());

            ParsedJoint pj;
            pj.name = jname;
            pj.parent = parent_idx;

            if (!ts.HasNext() || ts.next() != "{") {
                error = "Expected '{' after joint name: " + jname;
                return false;
            }

            // Read OFFSET
            if (!ts.HasNext() || ts.next() != "OFFSET") {
                error = "Expected 'OFFSET' for joint " + jname;
                return false;
            }
            try {
                pj.offset[0] = std::stof(ts.next());
                pj.offset[1] = std::stof(ts.next());
                pj.offset[2] = std::stof(ts.next());
            } catch (...) {
                error = "Invalid OFFSET floats in joint " + jname;
                return false;
            }

            // Read CHANNELS
            if (!ts.HasNext() || ts.next() != "CHANNELS") {
                error = "Expected 'CHANNELS' for joint " + jname;
                return false;
            }
            int num_channels = 0;
            try {
                num_channels = std::stoi(ts.next());
            } catch (...) {
                error = "Invalid channel count in joint " + jname;
                return false;
            }
            pj.channel_count = num_channels;
            for (int k = 0; k < num_channels; ++k) {
                if (!ts.HasNext()) {
                    error = "Unexpected EOF reading channels for " + jname;
                    return false;
                }
                pj.channel_types.push_back(ts.next());
            }

            joints.push_back(pj);
            joint_stack.push_back(new_idx);
        } else if (tok == "End" && ts.HasNext() && ts.peek() == "Site") {
            ts.next(); // consume "Site"
            if (!ts.HasNext() || ts.next() != "{") {
                error = "Expected '{' after End Site";
                return false;
            }
            if (!ts.HasNext() || ts.next() != "OFFSET") {
                error = "Expected 'OFFSET' for End Site";
                return false;
            }
            ts.next();
            ts.next();
            ts.next(); // consume 3 offset floats
            if (!ts.HasNext() || ts.next() != "}") {
                error = "Expected '}' for End Site";
                return false;
            }
        } else if (tok == "}") {
            if (!joint_stack.empty()) {
                joint_stack.pop_back();
            }
        }
    }

    const int J = static_cast<int>(joints.size());
    if (J == 0) {
        error = "No joints found in BVH";
        return false;
    }

    // Read MOTION section
    if (!ts.HasNext() || ts.next() != "Frames:") {
        error = "Expected 'Frames:' token in MOTION section";
        return false;
    }
    int frame_count = 0;
    try {
        frame_count = std::stoi(ts.next());
    } catch (...) {
        error = "Invalid frame count in MOTION section";
        return false;
    }

    if (!ts.HasNext() || ts.next() != "Frame" || !ts.HasNext() || ts.next() != "Time:") {
        error = "Expected 'Frame Time:' token";
        return false;
    }
    float frame_time = 0.033333f;
    try {
        frame_time = std::stof(ts.next());
    } catch (...) {
        error = "Invalid frame time";
        return false;
    }
    float fps = (frame_time > 1e-6f) ? (1.0f / frame_time) : 30.0f;

    // Allocate Animation
    out_animation.frames = frame_count;
    out_animation.joints = J;
    out_animation.fps = fps;
    out_animation.skeleton_name = "bvh-import";
    out_animation.joint_names.resize(J);
    out_animation.parents.resize(J);
    out_animation.offsets.resize(J);
    out_animation.root_positions.assign(static_cast<size_t>(frame_count) * 3, 0.0f);
    out_animation.local_rotations_xyzw.assign(static_cast<size_t>(frame_count) * J * 4, 0.0f);

    for (int j = 0; j < J; ++j) {
        out_animation.joint_names[j] = joints[j].name;
        out_animation.parents[j] = joints[j].parent;
        out_animation.offsets[j] = joints[j].offset;
    }

    // Read per-frame channel data
    for (int f = 0; f < frame_count; ++f) {
        for (int j = 0; j < J; ++j) {
            float px = 0.0f, py = 0.0f, pz = 0.0f;
            float rx = 0.0f, ry = 0.0f, rz = 0.0f;

            for (const std::string& ch : joints[j].channel_types) {
                if (!ts.HasNext()) {
                    error = "Unexpected EOF in frame " + std::to_string(f);
                    return false;
                }
                float val = 0.0f;
                try {
                    val = std::stof(ts.next());
                } catch (...) {
                    error = "Invalid motion float in frame " + std::to_string(f);
                    return false;
                }

                if (ch == "Xposition")
                    px = val;
                else if (ch == "Yposition")
                    py = val;
                else if (ch == "Zposition")
                    pz = val;
                else if (ch == "Xrotation")
                    rx = val;
                else if (ch == "Yrotation")
                    ry = val;
                else if (ch == "Zrotation")
                    rz = val;
            }

            if (j == 0) {
                out_animation.root_positions[f * 3 + 0] = px;
                out_animation.root_positions[f * 3 + 1] = py;
                out_animation.root_positions[f * 3 + 2] = pz;
            }

            float qx, qy, qz, qw;
            EulerXyzToQuat(rx, ry, rz, qx, qy, qz, qw);
            float* dst_q = out_animation.local_rotations_xyzw.data() + (static_cast<size_t>(f) * J + j) * 4;
            dst_q[0] = qx;
            dst_q[1] = qy;
            dst_q[2] = qz;
            dst_q[3] = qw;
        }
    }

    return true;
}

bool BVHParser::ParseFile(const std::string& file_path, Animation& out_animation, std::string& error) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        error = "Failed to open BVH file: " + file_path;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return ParseString(buffer.str(), out_animation, error);
}

BVHParser::ValidationReport BVHParser::Validate(const std::string& bvh_text) {
    ValidationReport rep;
    Animation anim;
    std::string err;
    if (!ParseString(bvh_text, anim, err)) {
        rep.valid = false;
        rep.errors.push_back(err);
        return rep;
    }

    rep.valid = true;
    rep.joint_count = anim.joints;
    rep.frame_count = anim.frames;
    rep.fps = anim.fps;
    rep.frame_time = (anim.fps > 0.0f) ? (1.0f / anim.fps) : 0.0f;
    rep.joint_names = anim.joint_names;

    // Check finite numbers
    for (float v : anim.root_positions) {
        if (!std::isfinite(v)) {
            rep.valid = false;
            rep.errors.push_back("Non-finite root position in BVH motion");
            break;
        }
    }
    for (float v : anim.local_rotations_xyzw) {
        if (!std::isfinite(v)) {
            rep.valid = false;
            rep.errors.push_back("Non-finite rotation quaternion in BVH motion");
            break;
        }
    }

    return rep;
}

} // namespace studio
