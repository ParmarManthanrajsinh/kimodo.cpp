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
            return studio::TestSuite::runAll();
        }
        if (arg == "--selftest-bvh") {
            return studio::TestSuite::runBVHRoundTrip();
        }
        if (arg == "--selftest-character") {
            return studio::TestSuite::runCharacterAndSkinning();
        }
        if (arg == "--selftest-soma") {
            return studio::TestSuite::runSomaPresentation();
        }
        if (arg == "--selftest-blender") {
            return studio::TestSuite::runBlenderRetargeting();
        }
        if (arg == "--screenshot") {
            studio::Application app;
            if (!app.init()) {
                return 1;
            }
            const char* outPath = (argc >= 3) ? argv[2] : "app_screenshot.png";
            app.run(15, outPath);
            app.shutdown();
            std::printf("Screenshot captured to %s\n", outPath);
            return 0;
        }
    }

    studio::Application app;
    if (!app.init()) {
        return 1;
    }
    app.run();
    app.shutdown();
    return 0;
}
