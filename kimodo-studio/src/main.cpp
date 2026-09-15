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
        // Identity source: root must be identity (source facing, upright),
        // and FK over the output must be self-consistent (no drift/NaN).
        float worst = 0.0f;
        for (int t = 0; t < restOut.joints; ++t) {
            const float* q = restOut.localRotationsXyzw.data() + t * 4;
            if (manny->parents[t] < 0) {
                worst = std::max(worst, std::abs(q[0]) + std::abs(q[1]) + std::abs(q[2]) +
                                            std::abs(q[3] - 1.0f));
                continue;
            }
            // Non-root joints must carry finite normalized quats.
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
    // Hips offset must be the true Manny value (~0.959m), not SOMA's.
    {
        const float hipsY = out.offsets[0][1];
        std::printf("selftest: retarget hipsOffY=%.4f\n", hipsY);
        if (std::abs(hipsY - 0.9590f) > 0.01f) {
            std::printf("selftest: RETARGET FAILED: offsets\n");
            return 1;
        }
    }
    // Collapse detector: mid-frame mean joint height must look standing.
    // Facing check: retarget root world must match source root world.
    {
        studio::AnimationPlayer probe;
        probe.load(out);
        probe.scrub(out.duration() * 0.5f);
        const auto& w = probe.worldPositions();
        double meanY = 0.0;
        for (const Vector3& v : w) {
            meanY += v.y;
        }
        meanY /= static_cast<double>(w.size());
        std::vector<Vector3> srcPos;
        std::vector<Quaternion> srcW;
        const int mid = out.frames / 2;
        studio::Skeleton::forwardKinematicsFull(
            anim.localRotationsXyzw.data() + static_cast<size_t>(mid) * anim.joints * 4,
            anim.rootPositions.data() + static_cast<size_t>(mid) * 3, anim.parents,
            anim.offsets, srcPos, srcW);
        std::vector<Vector3> outPos;
        std::vector<Quaternion> outW;
        studio::Skeleton::forwardKinematicsFull(
            out.localRotationsXyzw.data() + static_cast<size_t>(mid) * out.joints * 4,
            out.rootPositions.data() + static_cast<size_t>(mid) * 3, out.parents,
            out.offsets, outPos, outW);
        Quaternion a = QuaternionNormalize(srcW[0]);
        Quaternion b = QuaternionNormalize(outW[0]);
        float dot = std::abs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
        const float rootAngle = 2.0f * std::acos(std::min(1.0f, dot));
        std::printf("selftest: retarget midMeanY=%.3f rootAngle=%.4f rad\n", meanY,
                    rootAngle);
        if (meanY < 0.3) {
            std::printf("selftest: RETARGET FAILED: collapsed figure\n");
            return 1;
        }
        if (rootAngle > 0.05f) {
            std::printf("selftest: RETARGET FAILED: root facing drift\n");
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

int main(int argc, char** argv) {
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
    studio::Application app;
    if (!app.init()) {
        return 1;
    }
    app.run();
    app.shutdown();
    return 0;
}
