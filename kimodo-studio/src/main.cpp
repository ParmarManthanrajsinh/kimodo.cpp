#include "app/Application.h"
#include "tests/TestSuite.h"

#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    if (argc >= 2) {
        std::string arg = argv[1];
        if (arg == "--version" || arg == "-v") {
            std::printf("Kimodo Studio %s (%s) built %s\n", KIMODO_STUDIO_VERSION,
                        KIMODO_STUDIO_GIT_HASH, KIMODO_STUDIO_BUILD_DATE);
            return 0;
        }
        if (arg == "--selftest-all" || arg == "--selftest") {
            return studio::FTestSuite::RunAll();
        }
        if (arg == "--selftest-bvh") {
            return studio::FTestSuite::RunBVHRoundTrip();
        }
        if (arg == "--selftest-character") {
            return studio::FTestSuite::RunCharacterAndSkinning();
        }
        if (arg == "--selftest-soma") {
            return studio::FTestSuite::RunSomaPresentation();
        }
        if (arg == "--selftest-blender") {
            return studio::FTestSuite::RunBlenderRetargeting();
        }
        if (arg == "--screenshot") {
            const char* outPath = (argc >= 3) ? argv[2] : "app_screenshot.png";
            int w = (argc >= 5) ? std::atoi(argv[3]) : 1280;
            int h = (argc >= 5) ? std::atoi(argv[4]) : 800;
            studio::FApplication app;
            if (!app.Init(w, h)) {
                return 1;
            }
            app.Run(15, outPath);
            app.Shutdown();
            std::printf("Screenshot captured to %s (%dx%d)\n", outPath, w, h);
            return 0;
        }
    }

    studio::FApplication app;
    if (!app.Init()) {
        return 1;
    }
    app.Run();
    app.Shutdown();
    return 0;
}
