#include "animation/Animation.h"
#include "animation/AnimationPlayer.h"
#include "animation/Skeleton.h"
#include "app/Application.h"
#include "raymath.h"
#include "export/BVHExporter.h"
#include "export/ExportPreset.h"
#include "export/GLBExporter.h"
#include "retarget/Retargeter.h"
#include "retarget/SkeletonProfile.h"
#include "huggingface/HFAuthenticator.h"
#include "huggingface/HuggingFaceClient.h"
#include "kimodo/KimodoAdapter.h"
#include "library/AnimationLibrary.h"
#include "models/ModelManager.h"
#include "retarget/Retargeter.h"
#include "retarget/SkeletonProfile.h"
#include "utils/Logger.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
int selftest(const char* framesArg, const char* stepsArg) {
    studio::Logger::instance().init(studio::Logger::defaultLogFile());
    studio::ModelManager models;
    models.init(std::string(KIMODO_STUDIO_SOURCE_DIR) + "/config/models.json",
                "E:/kimodo.cpp/models",
                "E:/kimodo.cpp/generated/llm2vec-text-bundle");
    const auto entries = models.entries();
    std::printf("selftest: registry models=%llu active=%s\n",
                static_cast<unsigned long long>(entries.size()),
                models.activeId().c_str());
    for (const auto& e : entries) {
        std::printf("selftest: model %s installed=%d license=%s\n", e.id.c_str(),
                    e.installed ? 1 : 0, e.license.c_str());
    }
    if (entries.empty()) {
        std::printf("selftest: REGISTRY FAILED: no entries\n");
        return 1;
    }
    studio::KimodoAdapter adapter;
    int abi = studio::KimodoAdapter::abiVersion();
    std::printf("selftest: kimodo abi=%d\n", abi);
    std::string error;
    if (!adapter.load("E:/kimodo.cpp/models/kimodo-soma-rp-v1.1-f32.gguf",
                      "E:/kimodo.cpp/generated/llm2vec-text-bundle", error)) {
        std::printf("selftest: LOAD FAILED: %s\n", error.c_str());
        return 1;
    }
    std::printf("selftest: model loaded\n");
    studio::GenerationParams params;
    params.frames = framesArg ? static_cast<uint32_t>(std::stoul(framesArg)) : 60;
    params.steps = stepsArg ? static_cast<uint32_t>(std::stoul(stepsArg)) : 25;
    params.seed = 42;
    studio::MotionResult result;
    auto onProgress = [](unsigned done, unsigned total) {
        if (done == 1 || done == total || done % 5 == 0) {
            std::printf("selftest: progress %u/%u\n", done, total);
        }
        return false;
    };
    if (!adapter.generate("A person walks forward.", params, result, error, onProgress)) {
        std::printf("selftest: GENERATE FAILED: %s\n", error.c_str());
        return 1;
    }
    std::printf("selftest: OK frames=%d joints=%d rots=%llu roots=%llu\n", result.frames,
                result.joints,
                static_cast<unsigned long long>(result.localRotationsXyzw.size()),
                static_cast<unsigned long long>(result.rootPositions.size()));
    {
        size_t bad = 0;
        float nmin = 1e9f, nmax = 0.0f;
        for (size_t k = 0; k < result.localRotationsXyzw.size() / 4; ++k) {
            const float* q = result.localRotationsXyzw.data() + k * 4;
            for (int c = 0; c < 4; ++c) {
                if (!std::isfinite(q[c])) {
                    ++bad;
                }
            }
            const float n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] +
                                      q[3] * q[3]);
            nmin = std::min(nmin, n);
            nmax = std::max(nmax, n);
        }
        for (float v : result.rootPositions) {
            if (!std::isfinite(v)) {
                ++bad;
            }
        }
        std::printf("selftest: data nonfinite=%llu quatNormMin=%.6f quatNormMax=%.6f\n",
                    static_cast<unsigned long long>(bad), nmin, nmax);
    }
    if (!result.rootPositions.empty()) {
        std::printf("selftest: root0=%.4f %.4f %.4f\n", result.rootPositions[0],
                    result.rootPositions[1], result.rootPositions[2]);
    }
    // Phase 3: verify Animation + FK path (joint0 world == root0).
    studio::Animation anim;
    anim.fromMotionResult(result);
    studio::AnimationPlayer player;
    player.load(anim);
    player.scrub(0.0f);
    const auto& world = player.worldPositions();
    std::printf("selftest: player world joints=%llu frame=%d\n",
                static_cast<unsigned long long>(world.size()), player.frame());
    if (world.empty()) {
        std::printf("selftest: FK FAILED: no world positions\n");
        return 1;
    }
    const float dx = world[0].x - result.rootPositions[0];
    const float dy = world[0].y - result.rootPositions[1];
    const float dz = world[0].z - result.rootPositions[2];
    if (std::sqrt(dx * dx + dy * dy + dz * dz) > 1e-3f) {
        std::printf("selftest: FK FAILED: joint0 != root (%.4f %.4f %.4f)\n", world[0].x,
                    world[0].y, world[0].z);
        return 1;
    }
    player.scrub(anim.duration() * 0.5f);
    std::printf("selftest: mid scrub frame=%d hips=(%.3f %.3f %.3f)\n", player.frame(),
                player.worldPositions()[0].x, player.worldPositions()[0].y,
                player.worldPositions()[0].z);
    // Phase 9: retarget SOMA -> Manny, verify topology + rotation copy.
    const studio::SkeletonProfile* manny = studio::findProfile("unreal-manny");
    if (!manny) {
        std::printf("selftest: RETARGET FAILED: no manny profile\n");
        return 1;
    }
    studio::BoneMap map = studio::Retargeter::autoMap(*manny);
    const auto missing = studio::Retargeter::unmapped(*manny, map);
    studio::Animation out;
    std::string rerror;
    studio::Retargeter::Options ropts;
    if (!studio::Retargeter::retarget(anim, *manny, map, ropts, out, rerror)) {
        std::printf("selftest: RETARGET FAILED: %s\n", rerror.c_str());
        return 1;
    }
    std::printf("selftest: retarget joints=%d missing=%llu\n", out.joints,
                static_cast<unsigned long long>(missing.size()));
    if (out.joints != static_cast<int>(manny->joints.size()) ||
        out.frames != anim.frames) {
        std::printf("selftest: RETARGET FAILED: bad dims\n");
        return 1;
    }
    // Rest-pose assertion: FK over profile rest must stand tall
    // (head well above feet relative to root). Catches convention flips
    // in the emitted profile (direct = flat pancake when wrong).
    {
        const studio::SkeletonProfile* prof = manny;
        std::vector<float> restFlat(static_cast<size_t>(prof->joints.size()) * 4);
        for (size_t t = 0; t < prof->joints.size(); ++t) {
            restFlat[t * 4] = prof->restLocal[t][0];
            restFlat[t * 4 + 1] = prof->restLocal[t][1];
            restFlat[t * 4 + 2] = prof->restLocal[t][2];
            restFlat[t * 4 + 3] = prof->restLocal[t][3];
        }
        std::vector<Vector3> pos;
        std::vector<Quaternion> rot;
        const float org[3] = {0, 0, 0};
        studio::Skeleton::forwardKinematicsFull(restFlat.data(), org, prof->parents,
                                                prof->offsets, pos, rot);
        int hi = -1, fi = -1;
        for (size_t k = 0; k < prof->joints.size(); ++k) {
            if (prof->joints[k] == "head" || prof->joints[k] == "Head") {
                hi = static_cast<int>(k);
            }
            if (prof->joints[k] == "foot_l" || prof->joints[k] == "LeftFoot") {
                fi = static_cast<int>(k);
            }
        }
        if (hi < 0 || fi < 0) {
            std::printf("selftest: RETARGET FAILED: no head/foot joint\n");
            return 1;
        }
        const float span = pos[hi].y - pos[fi].y;
        std::printf("selftest: rest-pose headY=%.3f footY=%.3f span=%.3f\n", pos[hi].y,
                    pos[fi].y, span);
        if (span < 1.2f) {
            std::printf("selftest: RETARGET FAILED: rest pose not standing\n");
            return 1;
        }
    }
    // Identity source motion must reproduce the target rest exactly.
    {
        studio::Animation ident;
        ident.frames = 2;
        ident.joints = anim.joints;
        ident.fps = 30.0f;
        ident.skeletonName = "soma30";
        ident.jointNames = anim.jointNames;
        ident.parents = anim.parents;
        ident.offsets = anim.offsets;
        ident.localRotationsXyzw.assign(2 * anim.joints * 4, 0.0f);
        for (int k = 0; k < 2 * anim.joints; ++k) {
            ident.localRotationsXyzw[k * 4 + 3] = 1.0f;
        }
        ident.rootPositions.assign(6, 0.0f);
        studio::Animation restOut;
        std::string rerr;
        studio::Retargeter::Options ropts;
        if (!studio::Retargeter::retarget(ident, *manny, map, ropts, restOut, rerr)) {
            std::printf("selftest: RETARGET FAILED: identity %s\n", rerr.c_str());
            return 1;
        }
        // Identity source: root keeps source world (identity here), others
        // finite unit quats. Guards the root special-case against regressing
        // into offset math (which tips the figure).
        float worst = 0.0f;
        for (int t = 0; t < restOut.joints; ++t) {
            const float* q = restOut.localRotationsXyzw.data() + t * 4;
            if (manny->parents[t] < 0) {
                worst = std::max(worst, std::abs(q[0]) + std::abs(q[1]) +
                                            std::abs(q[2]) + std::abs(q[3] - 1.0f));
                continue;
            }
            const float n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] +
                                      q[3] * q[3]);
            if (!std::isfinite(n) || std::abs(n - 1.0f) > 1e-3f) {
                worst = 1.0f;
            }
        }
        std::printf("selftest: retarget identity worst=%.6f\n", worst);
        if (worst > 1e-4f) {
            std::printf("selftest: RETARGET FAILED: identity broken\n");
            return 1;
        }
    }
    // Pelvis offset must be the true Manny value (~0.959m), not SOMA's.
    // (UE5 hierarchy: root index 0 is the ground-level translation carrier;
    // pelvis index 1 carries the height.)
    {
        const float hipsY = out.offsets[1][1];
        std::printf("selftest: retarget hipsOffY=%.4f\n", hipsY);
        if (std::abs(hipsY - 0.9590f) > 0.01f) {
            std::printf("selftest: RETARGET FAILED: offsets\n");
            return 1;
        }
    }
    // Upright-figure check on the WALK retarget: head clearly above feet
    // through the clip (catches tipped-over output), plus forward travel.
    {
        int headIdx = -1, footL = -1, footR = -1;
        for (int k = 0; k < out.joints; ++k) {
            if (out.jointNames[k] == "head" || out.jointNames[k] == "Head") {
                headIdx = k;
            }
            if (out.jointNames[k] == "foot_l" || out.jointNames[k] == "LeftFoot") {
                footL = k;
            }
            if (out.jointNames[k] == "foot_r" || out.jointNames[k] == "RightFoot") {
                footR = k;
            }
        }
        if (headIdx < 0 || footL < 0 || footR < 0) {
            std::printf("selftest: RETARGET FAILED: missing head/feet\n");
            return 1;
        }
        int upright = 0;
        const int samples = 8;
        std::vector<Vector3> pos;
        std::vector<Quaternion> rot;
        for (int s = 0; s < samples; ++s) {
            const int f = (s * (out.frames - 1)) / (samples - 1);
            studio::Skeleton::forwardKinematicsFull(
                out.localRotationsXyzw.data() + static_cast<size_t>(f) * out.joints * 4,
                out.rootPositions.data() + static_cast<size_t>(f) * 3, out.parents,
                out.offsets, pos, rot);
            const float clearance =
                pos[headIdx].y - 0.5f * (pos[footL].y + pos[footR].y);
            (void)s;
            if (clearance > 0.8f) {
                ++upright;
            }
        }
        const float* r0 = out.rootPositions.data();
        const float* r1 = out.rootPositions.data() + (out.frames - 1) * 3;
        const float travel = std::sqrt((r1[0] - r0[0]) * (r1[0] - r0[0]) +
                                       (r1[2] - r0[2]) * (r1[2] - r0[2]));
        const float* s0 = anim.rootPositions.data();
        const float* s1 = anim.rootPositions.data() + (anim.frames - 1) * 3;
        // Facing check: retarget root yaw must track source root yaw
        // (catches constant about-face/moonwalk).
        auto yawOf = [](const float* q) {
            return std::atan2(2.0f * (q[3] * q[1] + q[0] * q[2]),
                              1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2]));
        };
        const int mf = out.frames / 2;
        const float* sqm = anim.localRotationsXyzw.data() +
                           static_cast<size_t>(mf) * anim.joints * 4;
        const float* oqm = out.localRotationsXyzw.data() +
                           static_cast<size_t>(mf) * out.joints * 4;
        // Root world yaw via FK.
        std::vector<Vector3> pA, pB;
        std::vector<Quaternion> qA, qB;
        studio::Skeleton::forwardKinematicsFull(
            sqm, s0, anim.parents, anim.offsets, pA, qA);
        studio::Skeleton::forwardKinematicsFull(
            oqm, r0, out.parents, out.offsets, pB, qB);
        float syaw = yawOf(reinterpret_cast<const float*>(&qA[0]));
        float tyaw = yawOf(reinterpret_cast<const float*>(&qB[0]));
        float yawDiff = std::abs(syaw - tyaw);
        if (yawDiff > 3.14159265f) {
            yawDiff = 2.0f * 3.14159265f - yawDiff;
        }
        std::printf("selftest: retarget upright %d/%d travel=%.3fm yawDiff=%.3f rad\n",
                    upright, samples, travel, yawDiff);
        std::printf("selftest: rootPath src=(%.3f %.3f)->(%.3f %.3f) tgt=(%.3f %.3f)->(%.3f %.3f)\n",
                    s0[0], s0[2], s1[0], s1[2], r0[0], r0[2], r1[0], r1[2]);
        if (yawDiff > 0.15f) {
            std::printf("selftest: RETARGET FAILED: facing mismatch\n");
            return 1;
        }
        if (upright < samples - 1) {
            std::printf("selftest: RETARGET FAILED: figure not upright\n");
            return 1;
        }
        if (travel < 0.2f) {
            std::printf("selftest: RETARGET FAILED: no forward travel\n");
            return 1;
        }
    }
    // Library round-trip: the exact Open/retarget path (save, rescan fresh,
    // load, compare, retarget the loaded copy).
    {
        const std::string base =
            std::string(std::getenv("TEMP") ? std::getenv("TEMP") : ".") + "/lib-selftest";
        std::error_code scopec;
        std::filesystem::remove_all(base, scopec);
        studio::AnimationLibrary lib;
        lib.init(base);
        studio::LibraryEntry saved;
        if (!lib.saveAnimation("probe walk", "soma-rp-v1.1", anim, saved)) {
            std::printf("selftest: LIB FAILED: save\n");
            return 1;
        }
        studio::AnimationLibrary lib2;
        lib2.init(base); // exercises metadata rescan parse
        const auto entries = lib2.entries();
        std::printf("selftest: lib entries=%llu\n",
                    static_cast<unsigned long long>(entries.size()));
        if (entries.size() != 1) {
            std::printf("selftest: LIB FAILED: rescan count\n");
            return 1;
        }
        studio::Animation reloaded;
        if (!lib2.loadAnimation(entries[0], reloaded)) {
            std::printf("selftest: LIB FAILED: load\n");
            return 1;
        }
        bool same = reloaded.frames == anim.frames && reloaded.joints == anim.joints &&
                    reloaded.fps == anim.fps &&
                    reloaded.jointNames == anim.jointNames &&
                    reloaded.parents == anim.parents &&
                    reloaded.offsets == anim.offsets &&
                    reloaded.localRotationsXyzw == anim.localRotationsXyzw &&
                    reloaded.rootPositions == anim.rootPositions &&
                    reloaded.skeletonName == anim.skeletonName;
        std::printf("selftest: lib roundtrip=%d\n", same ? 1 : 0);
        if (!same) {
            std::printf("selftest: LIB FAILED: data mismatch\n");
            return 1;
        }
        // Retarget the RELOADED copy (not the in-memory original).
        studio::Animation out2;
        std::string rerror2;
        if (!studio::Retargeter::retarget(reloaded, *manny, map, ropts, out2, rerror2)) {
            std::printf("selftest: LIB FAILED: retarget reloaded %s\n", rerror2.c_str());
            return 1;
        }
        if (out2.joints != static_cast<int>(manny->joints.size())) {
            std::printf("selftest: LIB FAILED: reloaded retarget dims\n");
            return 1;
        }
        std::printf("selftest: lib retarget-reloaded OK\n");
    }
    // Export the Manny retarget through the Unreal preset (user's exact path).
    {
        const studio::ExportPreset* unreal = studio::findPreset("unreal");
        studio::ExportOptions uo;
        uo.path = std::string(std::getenv("TEMP") ? std::getenv("TEMP") : ".") +
                  "/manny-selftest.glb";
        uo.fps = unreal->fps;
        uo.rootScale = unreal->scale;
        uo.basis = unreal->basis;
        std::string uerr;
        if (!studio::GLBExporter().exportAnimation(out, uo, uerr)) {
            std::printf("selftest: EXPORT FAILED: %s\n", uerr.c_str());
            return 1;
        }
        std::printf("selftest: exported %s\n", uo.path.c_str());
    }
    return 0;
}
} // namespace

int selftestHf() {
    studio::Logger::instance().init(studio::Logger::defaultLogFile());
    std::string error;
    // Bad token must fail cleanly (401 path).
    const std::string user =
        studio::HuggingFaceClient::whoami("hf_invalid_token_for_selftest", error);
    std::printf("selftest-hf: bad token user='%s' error='%s'\n", user.c_str(),
                error.c_str());
    if (!user.empty()) {
        std::printf("selftest-hf: FAILED: bad token accepted\n");
        return 1;
    }
    // Tiny real download through resolve redirect (SHA256SUMS, <1KB).
    const std::string url = studio::HuggingFaceClient::resolveUrl(
        "LocalAI-io/Kimodo-SOMA-RP-v1.1-GGML", "SHA256SUMS");
    const std::string tmp =
        std::string(std::getenv("TEMP") ? std::getenv("TEMP") : ".") + "/hf-selftest.txt";
    std::remove(tmp.c_str());
    if (!studio::HuggingFaceClient::download(
            url, tmp, "",
            [](uint64_t done, uint64_t total) {
                std::printf("selftest-hf: download %llu/%llu\n",
                            static_cast<unsigned long long>(done),
                            static_cast<unsigned long long>(total));
                return true;
            },
            error)) {
        std::printf("selftest-hf: DOWNLOAD FAILED: %s\n", error.c_str());
        return 1;
    }
    std::ifstream in(tmp);
    const std::string content{std::istreambuf_iterator<char>(in), {}};
    std::printf("selftest-hf: bytes=%llu head=%.64s\n",
                static_cast<unsigned long long>(content.size()), content.c_str());
    const bool ok = content.rfind(
                        "3bf1229f4c1eff1d28f5196a854113da2df9a11a5e21c60694630903bf948ee4",
                        0) == 0;
    std::remove(tmp.c_str());
    if (!ok) {
        std::printf("selftest-hf: FAILED: unexpected content\n");
        return 1;
    }
    std::printf("selftest-hf: OK\n");
    return 0;
}

int selftestExport(const char* keepPath = nullptr) {
    // Synthetic SOMA animation: no model needed.
    studio::Animation anim;
    anim.frames = 5;
    anim.joints = studio::kSomaJoints;
    anim.fps = 30.0f;
    anim.skeletonName = "soma30";
    anim.jointNames.assign(studio::Soma30Spec::names.begin(),
                           studio::Soma30Spec::names.end());
    anim.parents.assign(studio::Soma30Spec::parents.begin(),
                        studio::Soma30Spec::parents.end());
    anim.offsets.assign(studio::Soma30Spec::offsets.begin(),
                        studio::Soma30Spec::offsets.end());
    anim.localRotationsXyzw.resize(5 * 30 * 4);
    anim.rootPositions.resize(5 * 3);
    for (int f = 0; f < 5; ++f) {
        for (int j = 0; j < 30; ++j) {
            float* q = anim.localRotationsXyzw.data() + (f * 30 + j) * 4;
            q[0] = 0.01f * j;
            q[1] = 0.02f * f;
            q[2] = 0.0f;
            q[3] = 1.0f;
            const float n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[3] * q[3]);
            q[0] /= n;
            q[1] /= n;
            q[3] /= n;
        }
        float* p = anim.rootPositions.data() + f * 3;
        p[0] = 0.1f * f;
        p[1] = 0.97f;
        p[2] = 0.0f;
    }
    const std::string tmp =
        std::string(std::getenv("TEMP") ? std::getenv("TEMP") : ".") + "/glb-selftest.glb";
    studio::GLBExporter exporter;
    studio::ExportOptions opts;
    opts.path = tmp;
    std::string error;
    if (!exporter.exportAnimation(anim, opts, error)) {
        std::printf("selftest-glb: EXPORT FAILED: %s\n", error.c_str());
        return 1;
    }
    // Parse back: header, chunks, JSON markers, numeric payload.
    std::ifstream in(tmp, std::ios::binary);
    std::vector<char> data{std::istreambuf_iterator<char>(in), {}};
    auto u32 = [&data](size_t o) {
        return static_cast<uint32_t>(static_cast<unsigned char>(data[o])) |
               (static_cast<uint32_t>(static_cast<unsigned char>(data[o + 1])) << 8) |
               (static_cast<uint32_t>(static_cast<unsigned char>(data[o + 2])) << 16) |
               (static_cast<uint32_t>(static_cast<unsigned char>(data[o + 3])) << 24);
    };
    bool ok = data.size() > 20 && u32(0) == 0x46546C67 && u32(4) == 2;
    ok = ok && u32(8) == data.size(); // total length matches file size
    const uint32_t jsonLen = u32(12);
    const std::string json(data.data() + 20, jsonLen);
    size_t rotChannels = 0, pos = 0;
    while ((pos = json.find("\"path\":\"rotation\"", pos)) != std::string::npos) {
        ++rotChannels;
        ++pos;
    }
    ok = ok && rotChannels == 30;
    ok = ok && json.find("\"path\":\"translation\"") != std::string::npos;
    ok = ok && json.find("\"name\":\"LeftFoot\"") != std::string::npos;
    ok = ok && json.find("\"name\":\"KimodoClip\"") != std::string::npos;
    // BIN payload: times + joint0 quats + root.
    const size_t binStart = 20 + jsonLen + 8;
    auto f32 = [&data](size_t o) {
        float v = 0;
        std::memcpy(&v, data.data() + o, 4);
        return v;
    };
    ok = ok && f32(binStart) == 0.0f && f32(binStart + 4) == 1.0f / 30.0f;
    const size_t j0 = binStart + 5 * 4;
    const float* q0 = anim.localRotationsXyzw.data();
    ok = ok && f32(j0) == q0[0] && f32(j0 + 4) == q0[1] && f32(j0 + 12) == q0[3];
    const size_t rp = j0 + 30 * 5 * 16;
    ok = ok && f32(rp) == 0.0f && f32(rp + 4) == 0.97f;
    std::printf("selftest-glb: bytes=%llu rotChannels=%llu ok=%d\n",
                static_cast<unsigned long long>(data.size()),
                static_cast<unsigned long long>(rotChannels), ok ? 1 : 0);
    if (keepPath) {
        std::ifstream src(tmp, std::ios::binary);
        std::ofstream dst(keepPath, std::ios::binary | std::ios::trunc);
        dst << src.rdbuf();
        std::printf("selftest-glb: kept %s\n", keepPath);
        // Same clip through the Unreal preset (basis + cm scale).
        const studio::ExportPreset* unreal = studio::findPreset("unreal");
        studio::ExportOptions uo;
        uo.path = std::string(keepPath) + ".unreal.glb";
        uo.fps = 30.0f;
        uo.rootScale = unreal->scale;
        uo.basis = unreal->basis;
        std::string uerr;
        if (!exporter.exportAnimation(anim, uo, uerr)) {
            std::printf("selftest-glb: FAILED: unreal preset %s\n", uerr.c_str());
            return 1;
        }
        std::printf("selftest-glb: kept %s\n", uo.path.c_str());
    }
    std::remove(tmp.c_str());
    if (!ok) {
        std::printf("selftest-glb: FAILED: round-trip mismatch\n");
        return 1;
    }
    // Phase 11: preset basis math + resample.
    {
        const studio::ExportPreset* unreal = studio::findPreset("unreal");
        const studio::ExportPreset* unity = studio::findPreset("unity");
        if (!unreal || !unity) {
            std::printf("selftest-glb: FAILED: presets missing\n");
            return 1;
        }
        // Y-up (0,1,0) -> Unreal Z-up (0,0,1).
        const float ux = unreal->basis.m[0][1];
        const float uy = unreal->basis.m[1][1];
        const float uz = unreal->basis.m[2][1];
        // glTF +Z -> Unity -Z.
        const float nz = unity->basis.m[2][2];
        // Identity rotation survives basis round-trip.
        studio::Quat q{0.1f, 0.2f, 0.3f, 0.9f};
        q = studio::quatNormalize(q);
        studio::Mat3 r = studio::mat3FromQuat(q);
        studio::Quat back = studio::quatFromMat3(r);
        const float dq = std::abs(q.x - back.x) + std::abs(q.y - back.y) +
                         std::abs(q.z - back.z) + std::abs(q.w - back.w);
        std::printf("selftest-glb: unrealUp=(%.1f %.1f %.1f) unityZ=%.1f roundtrip=%.6f\n",
                    ux, uy, uz, nz, dq);
        if (ux != 0 || uy != 0 || uz != 1 || nz != -1 || dq > 1e-5f) {
            std::printf("selftest-glb: FAILED: preset math\n");
            return 1;
        }
        // Resample 5f@30 -> 10f@60 (duration preserved).
        anim.fps = 30.0f;
        studio::ExportOptions o60;
        o60.path = tmp;
        o60.fps = 60.0f;
        std::string e60;
        if (!exporter.exportAnimation(anim, o60, e60)) {
            std::printf("selftest-glb: FAILED: resample export %s\n", e60.c_str());
            return 1;
        }
        std::ifstream in60(tmp, std::ios::binary);
        std::vector<char> d60{std::istreambuf_iterator<char>(in60), {}};
        const uint32_t jlen60 = static_cast<uint32_t>(static_cast<unsigned char>(d60[12])) |
                                (static_cast<uint32_t>(static_cast<unsigned char>(d60[13])) << 8) |
                                (static_cast<uint32_t>(static_cast<unsigned char>(d60[14])) << 16) |
                                (static_cast<uint32_t>(static_cast<unsigned char>(d60[15])) << 24);
        const std::string j60(d60.data() + 20, jlen60);
        const size_t cp = j60.find("\"count\":"); // first accessor = times
        const int timesCount = cp == std::string::npos ? -1 : std::atoi(j60.c_str() + cp + 8);
        std::printf("selftest-glb: resampled times=%d\n", timesCount);
        std::remove(tmp.c_str());
        if (timesCount != 10) {
            std::printf("selftest-glb: FAILED: resample count\n");
            return 1;
        }
        // Phase 12: BVH export sanity (headers, frame count, euler of identity).
        studio::BVHExporter bvh;
        studio::ExportOptions bopts;
        bopts.path = tmp + ".bvh";
        bopts.fps = 30.0f;
        std::string berr;
        if (!bvh.exportAnimation(anim, bopts, berr)) {
            std::printf("selftest-glb: FAILED: bvh %s\n", berr.c_str());
            return 1;
        }
        std::ifstream bin(bopts.path);
        const std::string bvhText{std::istreambuf_iterator<char>(bin), {}};
        std::remove(bopts.path.c_str());
        const bool bH = bvhText.rfind("HIERARCHY\n", 0) == 0;
        const bool bR = bvhText.find("ROOT Hips\n") != std::string::npos;
        const bool bM = bvhText.find("MOTION\nFrames: 5\n") != std::string::npos;
        const bool bF = bvhText.find("Frame Time: ") != std::string::npos;
        // Frame 0 joint 0: root pos (0,0.97,0) + identity euler (0,0,0).
        const size_t mp = bvhText.find("MOTION\n");
        const size_t nl = bvhText.find('\n', bvhText.find("Frame Time: "));
        const std::string row = bvhText.substr(nl + 1, 64);
        // Note: atan2 can emit "-0"; accept both zero spellings.
        const bool bE = row.rfind("0 0.97 0 0 ", 0) == 0 &&
                        (row.rfind("0 0.97 0 0 0 ", 0) == 0 ||
                         row.rfind("0 0.97 0 0 -0 ", 0) == 0);
        std::printf("selftest-glb: bvh hier=%d root=%d motion=%d euler0=%d\n", bH, bR, bM,
                    bE);
        if (keepPath) {
            const std::string kept = std::string(keepPath) + ".check.bvh";
            std::ifstream bsrc(bopts.path, std::ios::binary);
            std::ofstream bdst(kept, std::ios::binary | std::ios::trunc);
            bdst << bsrc.rdbuf();
            std::printf("selftest-glb: kept %s\n", kept.c_str());
        }
        if (!bH || !bR || !bM || !bF || !bE) {
            std::printf("selftest-glb: FAILED: bvh content\n");
            return 1;
        }
    }
    std::printf("selftest-glb: OK\n");
    return 0;
}

namespace {

int synthFails = 0;

void checkSynth(bool ok, const char* name) {
    std::printf("selftest-chains: %s %s\n", ok ? "PASS" : "FAIL", name);
    if (!ok) {
        ++synthFails;
    }
}

int synthIndex(const studio::Animation& a, const char* name) {
    for (size_t i = 0; i < a.jointNames.size(); ++i) {
        if (a.jointNames[i] == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int synthIndex(const studio::SkeletonProfile& p, const char* name) {
    for (size_t i = 0; i < p.joints.size(); ++i) {
        if (p.joints[i] == name) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

studio::Animation makeSynthSoma(int frames = 3) {
    studio::Animation a;
    a.frames = frames;
    a.joints = studio::kSomaJoints;
    a.fps = 30.0f;
    a.skeletonName = "soma30";
    a.jointNames.assign(studio::Soma30Spec::names.begin(),
                        studio::Soma30Spec::names.end());
    a.parents.assign(studio::Soma30Spec::parents.begin(),
                     studio::Soma30Spec::parents.end());
    a.offsets.assign(studio::Soma30Spec::offsets.begin(),
                     studio::Soma30Spec::offsets.end());
    a.localRotationsXyzw.assign(static_cast<size_t>(frames) * 30 * 4, 0.0f);
    for (int k = 0; k < frames * 30; ++k) {
        a.localRotationsXyzw[k * 4 + 3] = 1.0f;
    }
    a.rootPositions.assign(static_cast<size_t>(frames) * 3, 0.0f);
    for (int f = 0; f < frames; ++f) {
        a.rootPositions[f * 3 + 1] = 0.9f;
    }
    return a;
}

void setSynthLocal(studio::Animation& a, const char* name, float x, float y, float z,
                   float w) {
    const int j = synthIndex(a, name);
    for (int f = 0; f < a.frames; ++f) {
        float* q = a.localRotationsXyzw.data() + (f * a.joints + j) * 4;
        q[0] = x;
        q[1] = y;
        q[2] = z;
        q[3] = w;
    }
}

float quatAngle(float ax, float ay, float az, float aw, float bx, float by, float bz,
                float bw) {
    float d = std::abs(ax * bx + ay * by + az * bz + aw * bw);
    d = std::min(1.0f, d);
    return 2.0f * std::acos(d);
}

float quatAngleArr(const float* a, const float* b) {
    return quatAngle(a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]);
}

float yawOfArr(const float* q) {
    return std::atan2(2.0f * (q[3] * q[1] + q[0] * q[2]),
                      1.0f - 2.0f * (q[1] * q[1] + q[2] * q[2]));
}

int selftestChains() {
    // Report header (req 15 inputs).
    const studio::SkeletonProfile* ue = studio::findProfile("unreal-manny");
    if (!ue) {
        std::printf("selftest-chains: FAIL no ue5 profile\n");
        return 1;
    }
    studio::BoneMap map = studio::Retargeter::autoMap(*ue);
    studio::Retargeter::Options opts;
    std::printf("selftest-chains: source soma30 joints=30\n");
    std::printf("selftest-chains: target %s joints=%llu chains=%llu\n", ue->id.c_str(),
                static_cast<unsigned long long>(ue->joints.size()),
                static_cast<unsigned long long>(ue->chains.size()));
    std::printf("selftest-chains: hasBind=%d off0=(%.3f %.3f %.3f) off1=(%.3f %.3f %.3f)\n",
                ue->hasBind ? 1 : 0, ue->offsets[0][0], ue->offsets[0][1],
                ue->offsets[0][2], ue->offsets[1][0], ue->offsets[1][1],
                ue->offsets[1][2]);
    int mappedChains = 0;
    for (const studio::ChainDef& c : ue->chains) {
        size_t m = 0;
        for (const std::string& t : c.target) {
            const auto it = map.find(t);
            if (it != map.end() && !it->second.empty() && it->second != "(none)") {
                ++m;
            }
        }
        std::printf("selftest-chains: chain %s mapped %llu/%llu\n", c.name.c_str(),
                    static_cast<unsigned long long>(m),
                    static_cast<unsigned long long>(c.target.size()));
        for (const std::string& t : c.target) {
            const auto it = map.find(t);
            std::printf("selftest-chains:   %s -> %s\n", t.c_str(),
                        it != map.end() ? it->second.c_str() : "(missing)");
        }
        if (m > 0) {
            ++mappedChains;
        }
    }
    std::printf("selftest-chains: unmapped joints=%llu\n",
                static_cast<unsigned long long>(
                    studio::Retargeter::unmapped(*ue, map).size()));

    // Basis unit tests (req 3).
    {
        studio::BasisConvert id = studio::BasisConvert::identity();
        const auto p = id.applyPos({1.0f, 2.0f, 3.0f}, 2.0f);
        checkSynth(p[0] == 2.0f && p[1] == 4.0f && p[2] == 6.0f, "basis identity pos");
        const auto q = id.applyQuat({0.0f, 0.0f, 0.0f, 1.0f});
        checkSynth(q[3] == 1.0f, "basis identity quat");
        studio::BasisConvert yz = studio::BasisConvert::yUpToZUp();
        const auto p2 = yz.applyPos({0.0f, 1.0f, 0.0f}, 1.0f);
        checkSynth(std::abs(p2[0]) < 1e-6f && std::abs(p2[1]) < 1e-6f &&
                       std::abs(p2[2] + 1.0f) < 1e-6f,
                   "basis yUpToZUp vector");
        const auto q2 = yz.applyQuat({0.0f, 0.0f, 0.0f, 1.0f});
        checkSynth(std::abs(q2[3] - 1.0f) < 1e-6f, "basis quat preserves identity");
    }

    // Identity source -> exact target rest (req 6 calibration baseline).
    {
        studio::Animation src = makeSynthSoma(2);
        studio::Animation out;
        std::string err;
        checkSynth(studio::Retargeter::retarget(src, *ue, map, opts, out, err),
                   "identity retarget runs");
        float worst = 0.0f;
        int worstJ = -1;
        for (int t = 0; t < out.joints; ++t) {
            if (ue->parents[t] < 0) {
                continue; // root keeps source world by design
            }
            const float* q = out.localRotationsXyzw.data() + t * 4;
            const auto& e = ue->restLocal[t];
            const float d = std::abs(q[0] - e[0]) + std::abs(q[1] - e[1]) +
                            std::abs(q[2] - e[2]) + std::abs(q[3] - e[3]);
            if (d > worst) {
                worst = d;
                worstJ = t;
            }
        }
        std::printf("selftest-chains: identity worst=%.6f joint=%s\n", worst,
                    worstJ >= 0 ? out.jointNames[worstJ].c_str() : "(none)");
        checkSynth(worst < 1e-4f, "identity reproduces target rest");
    }

    // Pure transfer opts (no IK): isolate rotation math from contact solve.
    studio::Retargeter::Options noIK = opts;
    noIK.legIK = false;

    // FK helper: world pos/rot of anim frame f with its own root.
    auto fkFrame = [](const studio::Animation& a, int f, std::vector<Vector3>& p,
                      std::vector<Quaternion>& r) {
        studio::Skeleton::forwardKinematicsFull(
            a.localRotationsXyzw.data() + static_cast<size_t>(f) * a.joints * 4,
            a.rootPositions.data() + static_cast<size_t>(f) * 3, a.parents,
            a.offsets, p, r);
    };
    // Target reference worlds from profile rest.
    std::vector<float> refFlat(static_cast<size_t>(ue->joints.size()) * 4);
    for (size_t t = 0; t < ue->joints.size(); ++t) {
        refFlat[t * 4] = ue->restLocal[t][0];
        refFlat[t * 4 + 1] = ue->restLocal[t][1];
        refFlat[t * 4 + 2] = ue->restLocal[t][2];
        refFlat[t * 4 + 3] = ue->restLocal[t][3];
    }
    studio::TargetReference tgtRef;
    {
        std::string referr;
        checkSynth(studio::buildTargetReference(*ue, tgtRef, referr),
                   "target reference builds (UE5 hierarchy verified)");
    }

    // STEP 3: reference pose in -> reference pose out. Compare WORLD
    // positions and rotations, not just normalized locals. Root excluded
    // from rotation check (carrier keeps source world); positions use the
    // retargeted root so translation convention cannot hide error.
    {
        studio::Animation src = makeSynthSoma(2);
        studio::Animation out;
        std::string err;
        checkSynth(studio::Retargeter::retarget(src, *ue, map, noIK, out, err),
                   "refpose retarget runs");
        std::vector<Vector3> pos;
        std::vector<Quaternion> rot;
        fkFrame(out, 0, pos, rot);
        // Reference FK under the same root as frame 0 output.
        std::vector<Vector3> rp;
        std::vector<Quaternion> rr;
        studio::Skeleton::forwardKinematicsFull(refFlat.data(),
                                                out.rootPositions.data(),
                                                out.parents, out.offsets, rp, rr);
        float worstP = 0.0f, worstR = 0.0f;
        for (int t = 0; t < out.joints; ++t) {
            worstP = std::max(worstP, static_cast<float>(Vector3Distance(pos[t], rp[t])));
            if (ue->parents[t] < 0) {
                continue;
            }
            const Quaternion& a = rot[t];
            const Quaternion& b = rr[t];
            worstR = std::max(worstR, std::abs(a.x - b.x) + std::abs(a.y - b.y) +
                                            std::abs(a.z - b.z) + std::abs(a.w - b.w));
        }
        std::printf("selftest-chains: refpose world worstP=%.6f worstR=%.6f\n", worstP,
                    worstR);
        checkSynth(worstP < 1e-4f, "refpose world positions preserved");
        checkSynth(worstR < 1e-4f, "refpose world rotations preserved");
    }

    // STEP 9: reference diagnostic. Makes frame errors obvious.
    {
        studio::Animation src = makeSynthSoma(1);
        std::vector<Vector3> sp;
        std::vector<Quaternion> sr;
        const float sroot[3] = {0.0f, 0.9f, 0.0f};
        std::vector<float> sident(static_cast<size_t>(src.joints) * 4, 0.0f);
        for (int k = 0; k < src.joints; ++k) {
            sident[k * 4 + 3] = 1.0f;
        }
        studio::Skeleton::forwardKinematicsFull(sident.data(), sroot, src.parents,
                                                src.offsets, sp, sr);
        const char* sNames[] = {"Hips",     "Spine1",   "Spine2",    "Chest",
                                "LeftShoulder", "LeftArm",    "LeftForeArm", "LeftFoot"};
        const char* tNames[] = {"pelvis",   "spine_01", "spine_03",  "spine_04",
                                "clavicle_l",   "upperarm_l", "forearm_l",   "foot_l"};
        for (int k = 0; k < 8; ++k) {
            const int si = synthIndex(src, sNames[k]);
            const int ti = synthIndex(*ue, tNames[k]);
            const Vector3& p0 = sp[si];
            const Quaternion& r0 = sr[si];
            const auto& p1 = tgtRef.worldPos[ti];
            const auto& r1 = tgtRef.worldRot[ti];
            std::printf("selftest-chains: ref %s S=(%.3f %.3f %.3f)/(%.3f %.3f %.3f %.3f) T %s=(%.3f %.3f %.3f)/(%.3f %.3f %.3f %.3f)\n",
                        sNames[k], p0.x, p0.y, p0.z, r0.x, r0.y, r0.z, r0.w, tNames[k],
                        p1[0], p1[1], p1[2], r1[0], r1[1], r1[2], r1[3]);
        }
        checkSynth(tgtRef.valid, "reference diagnostic printed");
    }

    // Pure root yaw 30deg -> target root yaw ~30deg (req 8 facing).
    {
        studio::Animation src = makeSynthSoma(2);
        const float h = 30.0f * 3.14159265f / 360.0f;
        setSynthLocal(src, "Hips", 0.0f, std::sin(h), 0.0f, std::cos(h));
        studio::Animation out;
        std::string err;
        studio::Retargeter::retarget(src, *ue, map, noIK, out, err);
        const float* q = out.localRotationsXyzw.data(); // root local == world
        const float yaw = yawOfArr(q) * 360.0f / (2.0f * 3.14159265f);
        checkSynth(std::abs(yaw - 30.0f) < 2.0f, "root yaw tracks source");
    }

    // Pelvis rotation only: 20deg X on Hips -> pelvis world delta 20deg,
    // spine_01 local stays rest (motion belongs to pelvis, not spine).
    {
        studio::Animation src = makeSynthSoma(2);
        const float h = 20.0f * 3.14159265f / 360.0f;
        setSynthLocal(src, "Hips", std::sin(h), 0.0f, 0.0f, std::cos(h));
        studio::Animation out;
        std::string err;
        studio::Retargeter::retarget(src, *ue, map, noIK, out, err);
        const int pel = synthIndex(out, "pelvis");
        const int s1 = synthIndex(out, "spine_01");
        std::vector<Vector3> pos;
        std::vector<Quaternion> rot;
        fkFrame(out, 0, pos, rot);
        const Quaternion& a = rot[pel];
        const auto& b = tgtRef.worldRot[pel];
        const float dw =
            quatAngle(a.x, a.y, a.z, a.w, b[0], b[1], b[2], b[3]) * 360.0f /
            (2.0f * 3.14159265f);
        // spine_01 WORLD must hold rest: its local counter-rotates against
        // the moved parent, which is correct (motion lives in pelvis world,
        // not duplicated as extra spine bend).
        const Quaternion& c = rot[s1];
        const auto& d = tgtRef.worldRot[s1];
        const float dw1 =
            quatAngle(c.x, c.y, c.z, c.w, d[0], d[1], d[2], d[3]) * 360.0f /
            (2.0f * 3.14159265f);
        std::printf("selftest-chains: pelvis worldDelta=%.1f spine01 worldHold=%.2f\n",
                    dw, dw1);
        checkSynth(std::abs(dw - 20.0f) < 2.0f, "pelvis rotation transfers");
        checkSynth(dw1 < 2.0f, "pelvis motion not duplicated into spine");
    }

    // Knee flex 45deg about X -> calf world delta ~45deg.
    {
        studio::Animation src = makeSynthSoma(2);
        const float h = 45.0f * 3.14159265f / 360.0f;
        setSynthLocal(src, "LeftShin", std::sin(h), 0.0f, 0.0f, std::cos(h));
        studio::Animation out;
        std::string err;
        studio::Retargeter::retarget(src, *ue, map, opts, out, err);
        const int calf = synthIndex(out, "calf_l");
        // Calf world vs rest world angle should mirror the 45deg input.
        std::vector<Vector3> pos;
        std::vector<Quaternion> rot;
        const float org[3] = {0, 0, 0};
        studio::Skeleton::forwardKinematicsFull(out.localRotationsXyzw.data(), org,
                                                out.parents, out.offsets, pos, rot);
        // Rest world of calf from profile.
        std::vector<float> rf(out.joints * 4);
        for (int t = 0; t < out.joints; ++t) {
            rf[t * 4] = ue->restLocal[t][0];
            rf[t * 4 + 1] = ue->restLocal[t][1];
            rf[t * 4 + 2] = ue->restLocal[t][2];
            rf[t * 4 + 3] = ue->restLocal[t][3];
        }
        std::vector<Vector3> rp;
        std::vector<Quaternion> rr;
        studio::Skeleton::forwardKinematicsFull(rf.data(), org, out.parents,
                                                out.offsets, rp, rr);
        const Quaternion& a = rot[calf];
        const Quaternion& b = rr[calf];
        const float ang =
            quatAngle(a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w) * 360.0f /
            (2.0f * 3.14159265f);
        checkSynth(std::abs(ang - 45.0f) < 5.0f, "knee flex transfers");
    }

    // Arm swing 30deg about Z -> upperarm world delta ~30deg.
    {
        studio::Animation src = makeSynthSoma(2);
        const float h = 30.0f * 3.14159265f / 360.0f;
        setSynthLocal(src, "LeftArm", 0.0f, 0.0f, std::sin(h), std::cos(h));
        studio::Animation out;
        std::string err;
        studio::Retargeter::retarget(src, *ue, map, opts, out, err);
        const int ua = synthIndex(out, "upperarm_l");
        std::vector<Vector3> pos;
        std::vector<Quaternion> rot;
        const float org[3] = {0, 0, 0};
        studio::Skeleton::forwardKinematicsFull(out.localRotationsXyzw.data(), org,
                                                out.parents, out.offsets, pos, rot);
        std::vector<float> rf(out.joints * 4);
        for (int t = 0; t < out.joints; ++t) {
            rf[t * 4] = ue->restLocal[t][0];
            rf[t * 4 + 1] = ue->restLocal[t][1];
            rf[t * 4 + 2] = ue->restLocal[t][2];
            rf[t * 4 + 3] = ue->restLocal[t][3];
        }
        std::vector<Vector3> rp;
        std::vector<Quaternion> rr;
        studio::Skeleton::forwardKinematicsFull(rf.data(), org, out.parents,
                                                out.offsets, rp, rr);
        const Quaternion& a = rot[ua];
        const Quaternion& b = rr[ua];
        const float ang =
            quatAngle(a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w) * 360.0f /
            (2.0f * 3.14159265f);
        checkSynth(std::abs(ang - 30.0f) < 5.0f, "arm swing transfers");
    }

    // Spine: Chest-only 30deg input must distribute over the shared span
    // (spine_04 + spine_05 world deltas sum to ~30, neither duplicates the
    // full rotation, neither drops to rest).
    {
        studio::Animation src = makeSynthSoma(2);
        const float h = 30.0f * 3.14159265f / 360.0f;
        setSynthLocal(src, "Chest", std::sin(h), 0.0f, 0.0f, std::cos(h));
        studio::Animation out;
        std::string err;
        studio::Retargeter::retarget(src, *ue, map, opts, out, err);
        const int s4 = synthIndex(out, "spine_04");
        const int s5 = synthIndex(out, "spine_05");
        std::vector<Vector3> pos;
        std::vector<Quaternion> rot;
        const float org[3] = {0, 0, 0};
        studio::Skeleton::forwardKinematicsFull(out.localRotationsXyzw.data(), org,
                                                out.parents, out.offsets, pos, rot);
        std::vector<float> rf(out.joints * 4);
        for (int t = 0; t < out.joints; ++t) {
            rf[t * 4] = ue->restLocal[t][0];
            rf[t * 4 + 1] = ue->restLocal[t][1];
            rf[t * 4 + 2] = ue->restLocal[t][2];
            rf[t * 4 + 3] = ue->restLocal[t][3];
        }
        std::vector<Vector3> rp;
        std::vector<Quaternion> rr;
        studio::Skeleton::forwardKinematicsFull(rf.data(), org, out.parents,
                                                out.offsets, rp, rr);
        auto wdeg = [&](int t) {
            const Quaternion& a = rot[t];
            const Quaternion& b = rr[t];
            return quatAngle(a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w) * 360.0f /
                   (2.0f * 3.14159265f);
        };
        const float d4 = wdeg(s4), d5 = wdeg(s5), d3 = wdeg(synthIndex(out, "spine_03"));
        std::printf("selftest-chains: spine s03=%.1f s04=%.1f s05=%.1f sum45=%.1f\n",
                    d3, d4, d5, d4 + d5);
        checkSynth(d3 < 2.0f, "spine unmapped source stays rest");
        checkSynth(d4 < 25.0f && d5 < 25.0f, "spine no full duplication");
        checkSynth(d4 > 3.0f && d5 > 3.0f, "spine span shared, none dropped");
        checkSynth(std::abs(d4 + d5 - 30.0f) < 4.0f, "spine motion conserved");
    }

    // Right arm: 20deg Z on RightArm -> upperarm_r world delta ~20deg.
    {
        studio::Animation src = makeSynthSoma(2);
        const float h = 20.0f * 3.14159265f / 360.0f;
        setSynthLocal(src, "RightArm", 0.0f, 0.0f, std::sin(h), std::cos(h));
        studio::Animation out;
        std::string err;
        studio::Retargeter::retarget(src, *ue, map, noIK, out, err);
        const int ur = synthIndex(out, "upperarm_r");
        std::vector<Vector3> pos;
        std::vector<Quaternion> rot;
        fkFrame(out, 0, pos, rot);
        const Quaternion& a = rot[ur];
        const auto& b = tgtRef.worldRot[ur];
        const float ang =
            quatAngle(a.x, a.y, a.z, a.w, b[0], b[1], b[2], b[3]) * 360.0f /
            (2.0f * 3.14159265f);
        checkSynth(std::abs(ang - 20.0f) < 3.0f, "right arm transfers");
    }

    // Right leg: 45deg X on RightShin -> calf_r world delta ~45deg.
    {
        studio::Animation src = makeSynthSoma(2);
        const float h = 45.0f * 3.14159265f / 360.0f;
        setSynthLocal(src, "RightShin", std::sin(h), 0.0f, 0.0f, std::cos(h));
        studio::Animation out;
        std::string err;
        studio::Retargeter::retarget(src, *ue, map, noIK, out, err);
        const int cr = synthIndex(out, "calf_r");
        std::vector<Vector3> pos;
        std::vector<Quaternion> rot;
        fkFrame(out, 0, pos, rot);
        const Quaternion& a = rot[cr];
        const auto& b = tgtRef.worldRot[cr];
        const float ang =
            quatAngle(a.x, a.y, a.z, a.w, b[0], b[1], b[2], b[3]) * 360.0f /
            (2.0f * 3.14159265f);
        checkSynth(std::abs(ang - 45.0f) < 5.0f, "right leg transfers");
    }

    // Combined walking pose (full opts incl. IK): yaw 10 + arm swings
    // +-15deg + left knee 25deg + forward root. Guards: all finite,
    // all normalized, head above feet (upright, limbs not inverted),
    // root yaw tracks source (no 180 flip).
    {
        studio::Animation src = makeSynthSoma(3);
        auto degAxis = [](float deg, float x, float y, float z) {
            const float h = deg * 3.14159265f / 360.0f;
            return std::array<float, 4>{x * std::sin(h), y * std::sin(h),
                                        z * std::sin(h), std::cos(h)};
        };
        const auto yaw = degAxis(10.0f, 0.0f, 1.0f, 0.0f);
        const auto swL = degAxis(15.0f, 0.0f, 0.0f, 1.0f);
        const auto swR = degAxis(-15.0f, 0.0f, 0.0f, 1.0f);
        const auto knee = degAxis(25.0f, 1.0f, 0.0f, 0.0f);
        setSynthLocal(src, "Hips", yaw[0], yaw[1], yaw[2], yaw[3]);
        setSynthLocal(src, "LeftArm", swL[0], swL[1], swL[2], swL[3]);
        setSynthLocal(src, "RightArm", swR[0], swR[1], swR[2], swR[3]);
        setSynthLocal(src, "LeftShin", knee[0], knee[1], knee[2], knee[3]);
        for (int f = 0; f < 3; ++f) {
            src.rootPositions[f * 3] = 0.3f * f;
            src.rootPositions[f * 3 + 1] = 0.9f - 0.02f * f;
        }
        studio::Animation out;
        std::string err;
        checkSynth(studio::Retargeter::retarget(src, *ue, map, opts, out, err),
                   "walk pose retarget runs");
        bool clean = true;
        for (int k = 0; k < out.frames * out.joints * 4; ++k) {
            if (!std::isfinite(out.localRotationsXyzw[k])) {
                clean = false;
            }
        }
        for (int t = 0; t < out.joints; ++t) {
            const float* q = out.localRotationsXyzw.data() + t * 4;
            const float n =
                std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
            if (!std::isfinite(n) || std::abs(n - 1.0f) > 1e-3f) {
                clean = false;
            }
        }
        checkSynth(clean, "walk pose finite and normalized, no NaN/Inf");
        const int hd = synthIndex(out, "head");
        const int fl = synthIndex(out, "foot_l");
        const int fr = synthIndex(out, "foot_r");
        const int th = synthIndex(out, "thigh_l");
        const int ca = synthIndex(out, "calf_l");
        const int pe = synthIndex(out, "pelvis");
        std::vector<Vector3> pos;
        std::vector<Quaternion> rot;
        fkFrame(out, 2, pos, rot);
        const float clearance =
            pos[hd].y - 0.5f * (pos[fl].y + pos[fr].y);
        const bool order = pos[pe].y > pos[th].y && pos[th].y > pos[ca].y &&
                           pos[ca].y > pos[fl].y;
        std::printf("selftest-chains: walk clearance=%.3f order=%d\n", clearance,
                    order ? 1 : 0);
        checkSynth(clearance > 0.8f, "walk pose upright");
        checkSynth(order, "walk pose limbs not inverted");
        const float* qr = out.localRotationsXyzw.data() + 2 * out.joints * 4;
        const float tyaw = yawOfArr(qr) * 360.0f / (2.0f * 3.14159265f);
        checkSynth(std::abs(tyaw - 10.0f) < 5.0f, "walk pose no facing flip");
    }

    // Calibration: scale 0 holds rest, 0.5 halves motion.
    {
        studio::Animation src = makeSynthSoma(2);
        const float h = 30.0f * 3.14159265f / 360.0f;
        setSynthLocal(src, "LeftArm", 0.0f, 0.0f, std::sin(h), std::cos(h));
        studio::Retargeter::Options o0 = opts;
        o0.legIK = false;
        o0.chains["LeftArm"] = studio::ChainParams{true, 0.0f};
        studio::Animation out0;
        std::string e0;
        studio::Retargeter::retarget(src, *ue, map, o0, out0, e0);
        const int ua = synthIndex(out0, "upperarm_l");
        const float* q0 = out0.localRotationsXyzw.data() + ua * 4;
        const auto& e = ue->restLocal[ua];
        // Component-wise: angle() is ill-conditioned near zero (acos
        // amplifies float noise); q0==e to 1e-7 here is exact hold.
        const float d0 = std::abs(q0[0] - e[0]) + std::abs(q0[1] - e[1]) +
                         std::abs(q0[2] - e[2]) + std::abs(q0[3] - e[3]);
        studio::Retargeter::Options o5 = opts;
        o5.chains["LeftArm"] = studio::ChainParams{true, 0.5f};
        studio::Animation out5;
        std::string e5;
        studio::Retargeter::retarget(src, *ue, map, o5, out5, e5);
        const float* q5 = out5.localRotationsXyzw.data() + ua * 4;
        const float d5 = quatAngle(q5[0], q5[1], q5[2], q5[3], e[0], e[1], e[2], e[3]);
        checkSynth(d0 < 1e-4f, "calibration scale 0 holds rest");
        checkSynth(std::abs(d5 - 15.0f * 3.14159265f / 180.0f) < 0.06f,
                   "calibration scale 0.5 halves motion");
    }

    // Mirrored limbs: identical inputs -> equal-magnitude outputs.
    {
        studio::Animation src = makeSynthSoma(2);
        const float h = 20.0f * 3.14159265f / 360.0f;
        setSynthLocal(src, "LeftArm", 0.0f, 0.0f, std::sin(h), std::cos(h));
        setSynthLocal(src, "RightArm", 0.0f, 0.0f, std::sin(h), std::cos(h));
        studio::Animation out;
        std::string err;
        studio::Retargeter::retarget(src, *ue, map, opts, out, err);
        const int ul = synthIndex(out, "upperarm_l");
        const int ur = synthIndex(out, "upperarm_r");
        const float* ql = out.localRotationsXyzw.data() + ul * 4;
        const float* qr = out.localRotationsXyzw.data() + ur * 4;
        const auto& el = ue->restLocal[ul];
        const auto& er = ue->restLocal[ur];
        const float dl = quatAngle(ql[0], ql[1], ql[2], ql[3], el[0], el[1], el[2], el[3]);
        const float dr = quatAngle(qr[0], qr[1], qr[2], qr[3], er[0], er[1], er[2], er[3]);
        bool finite = true;
        for (int k = 0; k < out.joints * 4; ++k) {
            if (!std::isfinite(out.localRotationsXyzw[k])) {
                finite = false;
            }
        }
        // Normalized check on a few joints.
        for (int t : {0, ul, ur}) {
            const float* q = out.localRotationsXyzw.data() + t * 4;
            const float n =
                std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
            if (!std::isfinite(n) || std::abs(n - 1.0f) > 1e-3f) {
                finite = false;
            }
        }
        checkSynth(finite, "outputs finite and normalized");
        checkSynth(std::abs(dl - dr) < 1e-3f, "mirrored limbs symmetric");
    }

    // Foot contact + IK: frame 0 stands (the rest reference, as in real
    // clips), frame 1 crouches with bent source knees and a dropped root;
    // target feet must stay near plant while knees flex.
    {
        studio::Animation src = makeSynthSoma(2);
        const float h = 30.0f * 3.14159265f / 360.0f;
        float* f1 = src.localRotationsXyzw.data() + 30 * 4;
        const int lsh = synthIndex(src, "LeftShin");
        const int rsh = synthIndex(src, "RightShin");
        f1[(lsh) * 4] = std::sin(h);
        f1[(lsh) * 4 + 3] = std::cos(h);
        f1[(rsh) * 4] = std::sin(h);
        f1[(rsh) * 4 + 3] = std::cos(h);
        src.rootPositions[3 + 1] = 0.83f; // frame 1 root drops 7cm
        studio::Animation out;
        std::string err;
        studio::Retargeter::retarget(src, *ue, map, opts, out, err);
        const int fl = synthIndex(out, "foot_l");
        const int calf = synthIndex(out, "calf_l");
        std::vector<Vector3> pos;
        std::vector<Quaternion> rot;
        const float org[3] = {0, 0, 0};
        // FK at frame 1 with its root.
        studio::Skeleton::forwardKinematicsFull(
            out.localRotationsXyzw.data() + out.joints * 4,
            out.rootPositions.data() + 3, out.parents, out.offsets, pos, rot);
        // Rest foot height reference.
        std::vector<float> rf(out.joints * 4);
        for (int t = 0; t < out.joints; ++t) {
            rf[t * 4] = ue->restLocal[t][0];
            rf[t * 4 + 1] = ue->restLocal[t][1];
            rf[t * 4 + 2] = ue->restLocal[t][2];
            rf[t * 4 + 3] = ue->restLocal[t][3];
        }
        std::vector<Vector3> rp;
        std::vector<Quaternion> rr;
        const float rorg[3] = {0, 0.9f, 0};
        studio::Skeleton::forwardKinematicsFull(rf.data(), rorg, out.parents,
                                                out.offsets, rp, rr);
        const float dy = std::abs(pos[fl].y - rp[fl].y);
        const Quaternion& a = rot[calf];
        const Quaternion& b = rr[calf];
        const float flex = quatAngle(a.x, a.y, a.z, a.w, b.x, b.y, b.z, b.w);
        std::printf("selftest-chains: crouch footDy=%.3f kneeFlex=%.1fdeg\n", dy,
                    flex * 360.0f / (2.0f * 3.14159265f));
        checkSynth(dy < 0.06f, "IK keeps foot contact on crouch");
        checkSynth(flex > 0.09f, "IK flexes knee on crouch");
    }

    // Root scale doubles translations.
    {
        studio::Animation src = makeSynthSoma(2);
        src.rootPositions[3] = 1.0f;
        src.rootPositions[5] = 2.0f;
        studio::Retargeter::Options o2 = opts;
        o2.rootScale = 2.0f;
        studio::Animation out;
        std::string err;
        studio::Retargeter::retarget(src, *ue, map, o2, out, err);
        checkSynth(std::abs(out.rootPositions[3] - 2.0f) < 1e-5f &&
                       std::abs(out.rootPositions[5] - 4.0f) < 1e-5f,
                   "root translation scale");
    }

    // Missing chain reports exactly (req 12).
    {
        studio::BoneMap bad;
        studio::Animation out;
        std::string err;
        const bool ok = studio::Retargeter::retarget(makeSynthSoma(2), *ue, bad, opts, out, err);
        checkSynth(!ok && err.find("LeftLeg") != std::string::npos, "missing chain error");
    }

    std::printf("selftest-chains: %s (%d failures)\n", synthFails == 0 ? "OK" : "FAILED",
                synthFails);
    return synthFails == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--version") {
        std::printf("Kimodo Studio %s (%s) built %s\n", KIMODO_STUDIO_VERSION,
                    KIMODO_STUDIO_GIT_HASH, KIMODO_STUDIO_BUILD_DATE);
        return 0;
    }
    if (argc >= 2 && std::string(argv[1]) == "--selftest") {
        const char* frames = argc >= 3 ? argv[2] : nullptr;
        const char* steps = argc >= 4 ? argv[3] : nullptr;
        return selftest(frames, steps);
    }
    if (argc >= 2 && std::string(argv[1]) == "--selftest-hf") {
        return selftestHf();
    }
    if (argc >= 2 && std::string(argv[1]) == "--selftest-export") {
        return selftestExport(argc >= 3 ? argv[2] : nullptr);
    }
    if (argc >= 2 && std::string(argv[1]) == "--selftest-chains") {
        return selftestChains();
    }
    studio::Application app;
    if (!app.init()) {
        return 1;
    }
    app.run();
    app.shutdown();
    return 0;
}
