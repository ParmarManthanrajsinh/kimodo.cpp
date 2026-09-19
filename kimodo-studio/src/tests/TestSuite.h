#pragma once

namespace studio {

class TestSuite {
public:
    static int RunAll();
    static int RunAnimationAndFK();
    static int RunSomaPresentation();
    static int RunCharacterAndSkinning();
    static int RunBVHRoundTrip();
    static int RunPathologicalCases();
    static int RunBlenderRetargeting();
};

} // namespace studio
