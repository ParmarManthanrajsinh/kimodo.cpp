#pragma once

namespace studio {

class FTestSuite {
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
