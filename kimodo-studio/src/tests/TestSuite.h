#pragma once

namespace studio {

class TestSuite {
public:
    static int runAll();
    static int runAnimationAndFK();
    static int runSomaPresentation();
    static int runCharacterAndSkinning();
    static int runBVHRoundTrip();
    static int runPathologicalCases();
    static int runBlenderRetargeting();
};

} // namespace studio
