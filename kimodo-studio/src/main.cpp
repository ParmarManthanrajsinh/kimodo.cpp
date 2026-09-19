#include "app/Application.h"
#include "tests/TestSuite.h"

#include <cstdio>
#include <string>

#ifndef KIMODO_STUDIO_VERSION
#define KIMODO_STUDIO_VERSION "0.1.0"
#endif

#ifndef KIMODO_STUDIO_GIT_HASH
#define KIMODO_STUDIO_GIT_HASH "dev"
#endif

#ifndef KIMODO_STUDIO_BUILD_DATE
#define KIMODO_STUDIO_BUILD_DATE "dev"
#endif

int main(int argc, char** argv) {
    if (argc >= 2) {
        std::string arg = argv[1];
        if (arg == "--version" || arg == "-v") {
            std::printf("Kimodo Studio %s (%s) built %s\n", KIMODO_STUDIO_VERSION, KIMODO_STUDIO_GIT_HASH,
                        KIMODO_STUDIO_BUILD_DATE);
            return 0;
        }
        if (arg == "--selftest-all" || arg == "--selftest") {
            return studio::TestSuite::RunAll();
        }
        if (arg == "--selftest-bvh") {
            return studio::TestSuite::RunBVHRoundTrip();
        }
        if (arg == "--selftest-character") {
            return studio::TestSuite::RunCharacterAndSkinning();
        }
        if (arg == "--selftest-soma") {
            return studio::TestSuite::RunSomaPresentation();
        }
        if (arg == "--selftest-blender") {
            return studio::TestSuite::RunBlenderRetargeting();
        }
        if (arg == "--screenshot") {
            const char* out_path = (argc >= 3) ? argv[2] : "app_screenshot.png";
            int w = (argc >= 5) ? std::atoi(argv[3]) : 1280;
            int h = (argc >= 5) ? std::atoi(argv[4]) : 800;
            studio::Application app;
            if (!app.Init(w, h)) {
                return 1;
            }
            app.Run(15, out_path);
            app.Shutdown();
            std::printf("Screenshot captured to %s (%dx%d)\n", out_path, w, h);
            return 0;
        }
    }

    studio::Application app;
    if (!app.Init()) {
        return 1;
    }
    app.Run();
    app.Shutdown();
    return 0;
}
