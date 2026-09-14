#include "app/Application.h"
#include "kimodo/KimodoAdapter.h"
#include "utils/Logger.h"

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
    if (!adapter.generate("A person walks forward.", params, result, error)) {
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
