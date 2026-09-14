#include "animation/Animation.h"
#include "animation/AnimationPlayer.h"
#include "app/Application.h"
#include "huggingface/HFAuthenticator.h"
#include "huggingface/HuggingFaceClient.h"
#include "kimodo/KimodoAdapter.h"
#include "models/ModelManager.h"
#include "utils/Logger.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

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

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--selftest") {
        const char* frames = argc >= 3 ? argv[2] : nullptr;
        const char* steps = argc >= 4 ? argv[3] : nullptr;
        return selftest(frames, steps);
    }
    if (argc >= 2 && std::string(argv[1]) == "--selftest-hf") {
        return selftestHf();
    }
    studio::Application app;
    if (!app.init()) {
        return 1;
    }
    app.run();
    app.shutdown();
    return 0;
}
