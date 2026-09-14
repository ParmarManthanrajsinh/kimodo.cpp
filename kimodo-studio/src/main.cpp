#include "animation/Animation.h"
#include "animation/AnimationPlayer.h"
#include "app/Application.h"
#include "kimodo/KimodoAdapter.h"
#include "utils/Logger.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {
int selftest(const char* framesArg, const char* stepsArg) {
    studio::Logger::instance().init(studio::Logger::defaultLogFile());
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

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--selftest") {
        const char* frames = argc >= 3 ? argv[2] : nullptr;
        const char* steps = argc >= 4 ? argv[3] : nullptr;
        return selftest(frames, steps);
    }
    studio::Application app;
    if (!app.init()) {
        return 1;
    }
    app.run();
    app.shutdown();
    return 0;
}
