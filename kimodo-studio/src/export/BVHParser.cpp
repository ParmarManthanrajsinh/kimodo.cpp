#include "export/BVHParser.h"
#include "raymath.h"

#include <cmath>
#include <fstream>
#include <sstream>

namespace studio {

void BVHParser::eulerXYZToQuat(float ex, float ey, float ez,
                               float& x, float& y, float& z, float& w) {
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

    bool hasNext() const { return cursor < tokens.size(); }
    const std::string& next() { return tokens[cursor++]; }
    const std::string& peek() const { return tokens[cursor]; }
    void backtrack() { if (cursor > 0) cursor--; }
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
    int channelCount = 0;
    std::vector<std::string> channelTypes;
};

} // namespace

bool BVHParser::parseString(const std::string& bvhText, Animation& outAnimation,
                            std::string& error) {
    TokenStream ts = tokenize(bvhText);
    if (!ts.hasNext()) {
        error = "BVH text is empty";
        return false;
    }

    if (ts.next() != "HIERARCHY") {
        error = "Expected 'HIERARCHY' token";
        return false;
    }

    std::vector<ParsedJoint> joints;
    std::vector<int> jointStack;

    // Parse HIERARCHY section
    while (ts.hasNext()) {
        const std::string tok = ts.next();
        if (tok == "MOTION") {
            break;
        }

        if (tok == "ROOT" || tok == "JOINT") {
            if (!ts.hasNext()) {
                error = "Unexpected end of tokens after " + tok;
                return false;
            }
            std::string jname = ts.next();
            int parentIdx = jointStack.empty() ? -1 : jointStack.back();
            int newIdx = static_cast<int>(joints.size());

            ParsedJoint pj;
            pj.name = jname;
            pj.parent = parentIdx;

            if (!ts.hasNext() || ts.next() != "{") {
                error = "Expected '{' after joint name: " + jname;
                return false;
            }

            // Read OFFSET
            if (!ts.hasNext() || ts.next() != "OFFSET") {
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
            if (!ts.hasNext() || ts.next() != "CHANNELS") {
                error = "Expected 'CHANNELS' for joint " + jname;
                return false;
            }
            int numChannels = 0;
            try {
                numChannels = std::stoi(ts.next());
            } catch (...) {
                error = "Invalid channel count in joint " + jname;
                return false;
            }
            pj.channelCount = numChannels;
            for (int k = 0; k < numChannels; ++k) {
                if (!ts.hasNext()) {
                    error = "Unexpected EOF reading channels for " + jname;
                    return false;
                }
                pj.channelTypes.push_back(ts.next());
            }

            joints.push_back(pj);
            jointStack.push_back(newIdx);
        } else if (tok == "End" && ts.hasNext() && ts.peek() == "Site") {
            ts.next(); // consume "Site"
            if (!ts.hasNext() || ts.next() != "{") {
                error = "Expected '{' after End Site";
                return false;
            }
            if (!ts.hasNext() || ts.next() != "OFFSET") {
                error = "Expected 'OFFSET' for End Site";
                return false;
            }
            ts.next(); ts.next(); ts.next(); // consume 3 offset floats
            if (!ts.hasNext() || ts.next() != "}") {
                error = "Expected '}' for End Site";
                return false;
            }
        } else if (tok == "}") {
            if (!jointStack.empty()) {
                jointStack.pop_back();
            }
        }
    }

    const int J = static_cast<int>(joints.size());
    if (J == 0) {
        error = "No joints found in BVH";
        return false;
    }

    // Read MOTION section
    if (!ts.hasNext() || ts.next() != "Frames:") {
        error = "Expected 'Frames:' token in MOTION section";
        return false;
    }
    int frameCount = 0;
    try {
        frameCount = std::stoi(ts.next());
    } catch (...) {
        error = "Invalid frame count in MOTION section";
        return false;
    }

    if (!ts.hasNext() || ts.next() != "Frame" || !ts.hasNext() || ts.next() != "Time:") {
        error = "Expected 'Frame Time:' token";
        return false;
    }
    float frameTime = 0.033333f;
    try {
        frameTime = std::stof(ts.next());
    } catch (...) {
        error = "Invalid frame time";
        return false;
    }
    float fps = (frameTime > 1e-6f) ? (1.0f / frameTime) : 30.0f;

    // Allocate Animation
    outAnimation.frames = frameCount;
    outAnimation.joints = J;
    outAnimation.fps = fps;
    outAnimation.skeletonName = "bvh-import";
    outAnimation.jointNames.resize(J);
    outAnimation.parents.resize(J);
    outAnimation.offsets.resize(J);
    outAnimation.rootPositions.assign(static_cast<size_t>(frameCount) * 3, 0.0f);
    outAnimation.localRotationsXyzw.assign(static_cast<size_t>(frameCount) * J * 4, 0.0f);

    for (int j = 0; j < J; ++j) {
        outAnimation.jointNames[j] = joints[j].name;
        outAnimation.parents[j] = joints[j].parent;
        outAnimation.offsets[j] = joints[j].offset;
    }

    // Read per-frame channel data
    for (int f = 0; f < frameCount; ++f) {
        for (int j = 0; j < J; ++j) {
            float px = 0.0f, py = 0.0f, pz = 0.0f;
            float rx = 0.0f, ry = 0.0f, rz = 0.0f;

            for (const std::string& ch : joints[j].channelTypes) {
                if (!ts.hasNext()) {
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

                if (ch == "Xposition") px = val;
                else if (ch == "Yposition") py = val;
                else if (ch == "Zposition") pz = val;
                else if (ch == "Xrotation") rx = val;
                else if (ch == "Yrotation") ry = val;
                else if (ch == "Zrotation") rz = val;
            }

            if (j == 0) {
                outAnimation.rootPositions[f * 3 + 0] = px;
                outAnimation.rootPositions[f * 3 + 1] = py;
                outAnimation.rootPositions[f * 3 + 2] = pz;
            }

            float qx, qy, qz, qw;
            eulerXYZToQuat(rx, ry, rz, qx, qy, qz, qw);
            float* dstQ = outAnimation.localRotationsXyzw.data() + (static_cast<size_t>(f) * J + j) * 4;
            dstQ[0] = qx;
            dstQ[1] = qy;
            dstQ[2] = qz;
            dstQ[3] = qw;
        }
    }

    return true;
}

bool BVHParser::parseFile(const std::string& filePath, Animation& outAnimation,
                          std::string& error) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        error = "Failed to open BVH file: " + filePath;
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return parseString(buffer.str(), outAnimation, error);
}

BVHParser::ValidationReport BVHParser::validate(const std::string& bvhText) {
    ValidationReport rep;
    Animation anim;
    std::string err;
    if (!parseString(bvhText, anim, err)) {
        rep.valid = false;
        rep.errors.push_back(err);
        return rep;
    }

    rep.valid = true;
    rep.jointCount = anim.joints;
    rep.frameCount = anim.frames;
    rep.fps = anim.fps;
    rep.frameTime = (anim.fps > 0.0f) ? (1.0f / anim.fps) : 0.0f;
    rep.jointNames = anim.jointNames;

    // Check finite numbers
    for (float v : anim.rootPositions) {
        if (!std::isfinite(v)) {
            rep.valid = false;
            rep.errors.push_back("Non-finite root position in BVH motion");
            break;
        }
    }
    for (float v : anim.localRotationsXyzw) {
        if (!std::isfinite(v)) {
            rep.valid = false;
            rep.errors.push_back("Non-finite rotation quaternion in BVH motion");
            break;
        }
    }

    return rep;
}

} // namespace studio
