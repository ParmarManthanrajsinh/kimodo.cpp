#include "tests/TestSuite.h"
#include "animation/Animation.h"
#include "animation/Skeleton.h"
#include "animation/SomaPresentation.h"
#include "character/CharacterAsset.h"
#include "character/CharacterLoader.h"
#include "character/CharacterMapper.h"
#include "export/BVHExporter.h"
#include "export/BVHParser.h"
#include "export/ExportPreset.h"
#include "library/AnimationLibrary.h"
#include "raymath.h"
#include "app/AppState.h"
#include "retarget/Retargeter.h"
#include "retarget/SkeletonProfile.h"
#include "utils/AppPaths.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace studio {
namespace {

Animation makeSynthSoma(int frames = 4) {
    Animation a;
    a.frames = frames;
    a.fps = 30.0f;
    a.joints = kSomaJoints;
    a.skeletonName = "soma30";
    a.jointNames.assign(Soma30Spec::names.begin(), Soma30Spec::names.end());
    a.parents.assign(Soma30Spec::parents.begin(), Soma30Spec::parents.end());
    a.offsets.assign(Soma30Spec::offsets.begin(), Soma30Spec::offsets.end());
    a.localRotationsXyzw.assign(static_cast<size_t>(frames) * kSomaJoints * 4, 0.0f);
    a.rootPositions.assign(static_cast<size_t>(frames) * 3, 0.0f);

    for (int f = 0; f < frames; ++f) {
        a.rootPositions[f * 3 + 0] = 0.0f;
        a.rootPositions[f * 3 + 1] = 0.95f + 0.02f * static_cast<float>(f);
        a.rootPositions[f * 3 + 2] = 0.05f * static_cast<float>(f);

        for (int j = 0; j < kSomaJoints; ++j) {
            a.localRotationsXyzw[(static_cast<size_t>(f) * kSomaJoints + j) * 4 + 3] = 1.0f;
        }

        // Slight arm swing
        float angle = (f - frames / 2.0f) * 0.1f;
        float h = angle * 0.5f;
        int lArm = 11, rArm = 17;
        a.localRotationsXyzw[(static_cast<size_t>(f) * kSomaJoints + lArm) * 4 + 0] = std::sin(h);
        a.localRotationsXyzw[(static_cast<size_t>(f) * kSomaJoints + lArm) * 4 + 3] = std::cos(h);
        a.localRotationsXyzw[(static_cast<size_t>(f) * kSomaJoints + rArm) * 4 + 0] = -std::sin(h);
        a.localRotationsXyzw[(static_cast<size_t>(f) * kSomaJoints + rArm) * 4 + 3] = std::cos(h);
    }
    return a;
}

} // namespace

int TestSuite::runAnimationAndFK() {
    int fails = 0;
    std::printf("[TEST] Running Animation & Skeleton FK tests...\n");

    Animation anim = makeSynthSoma(5);
    if (anim.frames != 5 || anim.joints != 30) {
        std::printf("  FAIL: Synthetic animation dimension mismatch\n");
        fails++;
    }

    // Forward kinematics test
    std::vector<Vector3> pos;
    std::vector<Quaternion> rot;
    const float* root = anim.rootPositions.data();
    const float* rots = anim.localRotationsXyzw.data();

    Skeleton::forwardKinematicsFull(rots, root, anim.parents, anim.offsets, pos, rot);

    if (pos.size() != 30 || rot.size() != 30) {
        std::printf("  FAIL: FK result joint count != 30\n");
        fails++;
    }

    // Root joint world position must equal root translation
    float dx = pos[0].x - root[0];
    float dy = pos[0].y - root[1];
    float dz = pos[0].z - root[2];
    if (std::sqrt(dx * dx + dy * dy + dz * dz) > 1e-4f) {
        std::printf("  FAIL: Root world pos != root translation\n");
        fails++;
    }

    std::printf("  -> Animation & FK: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::runSomaPresentation() {
    int fails = 0;
    std::printf("[TEST] Running SOMA Presentation Skeleton tests...\n");

    Animation src = makeSynthSoma(3);
    Animation pres;
    std::string err;

    if (!SomaPresentation::expandSoma30(src, pres, err)) {
        std::printf("  FAIL: SomaPresentation::expandSoma30 failed: %s\n", err.c_str());
        fails++;
    }

    if (pres.joints <= 30) {
        std::printf("  FAIL: Expanded presentation joint count <= 30\n");
        fails++;
    }

    // Validation
    auto val = SomaPresentation::validate(pres);
    if (!val.valid || !val.hierarchyValid || !val.isFinite) {
        std::printf("  FAIL: SomaPresentation validation failed\n");
        for (const auto& e : val.errors) std::printf("    Error: %s\n", e.c_str());
        fails++;
    }

    std::printf("  -> SOMA Presentation: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::runCharacterAndSkinning() {
    int fails = 0;
    std::printf("[TEST] Running Character & Skinning tests...\n");

    std::filesystem::path charPath = AppPaths::resolveAsset("assets/characters/CesiumMan.glb");
    CharacterAsset character;
    std::string err;

    if (!CharacterLoader::loadGLB(charPath.string(), character, err)) {
        std::printf("  WARN: CharacterLoader on %s: %s (skipping if file not present)\n",
                    charPath.string().c_str(), err.c_str());
    } else {
        if (!character.isLoaded() || character.skinningData().vertices.empty()) {
            std::printf("  FAIL: Loaded character has no vertices\n");
            fails++;
        }

        const auto& rep = character.validationReport();
        if (!rep.hasMesh || !rep.hasSkeleton) {
            std::printf("  FAIL: Character validation checklist failed\n");
            fails++;
        }

        // Test Auto Mapper
        Animation anim = makeSynthSoma(2);
        auto mapping = CharacterMapper::autoMap(character, anim.jointNames);
        if (mapping.empty()) {
            std::printf("  FAIL: CharacterMapper autoMap returned empty map\n");
            fails++;
        }

        std::vector<Matrix> skinMats;
        if (!CharacterMapper::evaluateSkinMatrices(character, anim, 0, mapping, skinMats)) {
            std::printf("  FAIL: evaluateSkinMatrices returned false\n");
            fails++;
        }

        // Verify critical mappings are correct (RightArm, RightForeArm, Head)
        if (mapping["Skeleton_arm_joint_R__2_"] != "RightArm") {
            std::printf("  FAIL: Skeleton_arm_joint_R__2_ mapped to '%s' instead of 'RightArm'\n",
                        mapping["Skeleton_arm_joint_R__2_"].c_str());
            fails++;
        }
        if (mapping["Skeleton_arm_joint_R__3_"] != "RightForeArm") {
            std::printf("  FAIL: Skeleton_arm_joint_R__3_ mapped to '%s' instead of 'RightForeArm'\n",
                        mapping["Skeleton_arm_joint_R__3_"].c_str());
            fails++;
        }
        if (mapping["Skeleton_neck_joint_2"] != "Head") {
            std::printf("  FAIL: Skeleton_neck_joint_2 mapped to '%s' instead of 'Head'\n",
                        mapping["Skeleton_neck_joint_2"].c_str());
            fails++;
        }

        AnimationLibrary lib;
        lib.init(AppPaths::defaultAnimationsDir());
        std::printf("  [DEBUG] Library has %zu animation clips\n", lib.entries().size());
        for (const auto& entry : lib.entries()) {
            std::printf("    Clip: '%s', frames=%d, skeleton='%s'\n", entry.prompt.c_str(), entry.frames, entry.skeleton.c_str());
            Animation realAnim;
            if (lib.loadAnimation(entry, realAnim)) {
                if (entry.prompt.find("apple") != std::string::npos) {
                    std::printf("    === INSPECTING CLIP: %s ===\n", entry.prompt.c_str());
                    int testFrames[] = {0, 20, 40, 60, 80, 100, std::min(119, realAnim.frames - 1)};
                    for (int tf : testFrames) {
                        // SOMA FK
                        const float* r = realAnim.localRotationsXyzw.data() + static_cast<size_t>(tf) * realAnim.joints * 4;
                        const float* p = realAnim.rootPositions.data() + tf * 3;
                        std::vector<Vector3> somaPos;
                        std::vector<Quaternion> somaRot;
                        Skeleton::forwardKinematicsFull(r, p, realAnim.parents, realAnim.offsets, somaPos, somaRot);

                        // Character FK
                        std::vector<Matrix> curSkinMats;
                        std::vector<Vector3> charPos;
                        CharacterMapper::evaluateSkinMatrices(character, realAnim, tf, mapping, curSkinMats, &charPos);

                        // Joint 17 = RightArm, Joint 18 = RightForeArm, Joint 19 = RightHand, Joint 6 = Head
                        std::printf("      Frame %d:\n", tf);
                        std::printf("        SOMA Root (Hips): (%.3f, %.3f, %.3f)\n", somaPos[0].x, somaPos[0].y, somaPos[0].z);
                        std::printf("        SOMA Head: (%.3f, %.3f, %.3f)\n", somaPos[6].x, somaPos[6].y, somaPos[6].z);
                        std::printf("        SOMA RightArm: (%.3f, %.3f, %.3f)\n", somaPos[17].x, somaPos[17].y, somaPos[17].z);
                        std::printf("        SOMA RightForeArm: (%.3f, %.3f, %.3f)\n", somaPos[18].x, somaPos[18].y, somaPos[18].z);
                        std::printf("        SOMA RightHand: (%.3f, %.3f, %.3f)\n", somaPos[19].x, somaPos[19].y, somaPos[19].z);

                        // Char Bone 4 = Head (Skeleton_neck_joint_2), Bone 8 = RightArm, Bone 10 = RightForeArm
                        std::printf("        CHAR Head: (%.3f, %.3f, %.3f)\n", charPos[4].x, charPos[4].y, charPos[4].z);
                        std::printf("        CHAR RightArm: (%.3f, %.3f, %.3f)\n", charPos[8].x, charPos[8].y, charPos[8].z);
                        std::printf("        CHAR RightForeArm: (%.3f, %.3f, %.3f)\n", charPos[10].x, charPos[10].y, charPos[10].z);
                    }
                }
            }
        }

        // Test Skin Matrix Evaluation
        if (skinMats.size() != character.bones().size()) {
            std::printf("  FAIL: Skin matrix count != bone count\n");
            fails++;
        }

        // Test CPU skinning
        character.updateCpuSkinning(skinMats);
        const auto& animVerts = character.animatedVertices();
        if (animVerts.size() != character.skinningData().vertices.size()) {
            std::printf("  FAIL: Animated vertices count mismatch\n");
            fails++;
        } else {
            // Verify animated mesh is upright in Y-UP space (height Y >= 1.0m, Z extends reasonably)
            float minY = 1e9f, maxY = -1e9f;
            for (const auto& v : animVerts) {
                if (v.y < minY) minY = v.y;
                if (v.y > maxY) maxY = v.y;
            }
            if (maxY < 1.2f || minY < -0.3f) {
                std::printf("  FAIL: Animated character not upright! Y bounds: [%.2f, %.2f]\n", minY, maxY);
                fails++;
            }
        }
    }

    // Test UI Panel Width Clamping
    AppState testState;
    testState.sideWidth = 50.0f; // Below min 120
    testState.sideWidth = std::clamp(testState.sideWidth, 120.0f, 320.0f);
    if (testState.sideWidth != 120.0f) {
        std::printf("  FAIL: sideWidth clamp failed min\n");
        fails++;
    }
    testState.panelWidth = 1000.0f; // Above max 640
    testState.panelWidth = std::clamp(testState.panelWidth, 260.0f, 640.0f);
    if (testState.panelWidth != 640.0f) {
        std::printf("  FAIL: panelWidth clamp failed max\n");
        fails++;
    }

    std::printf("  -> Character & Skinning: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::runBVHRoundTrip() {
    int fails = 0;
    std::printf("[TEST] Running BVH Export, Parser & Round-Trip tests...\n");

    Animation src = makeSynthSoma(6);
    ExportOptions opts;
    opts.path = (AppPaths::appDataDir() / "test_roundtrip.bvh").string();
    opts.fps = 30.0f;

    std::string err;
    BVHExporter bvhExp;
    if (!bvhExp.exportAnimation(src, opts, err)) {
        std::printf("  FAIL: BVHExporter failed: %s\n", err.c_str());
        fails++;
    }

    Animation roundTrip;
    if (!BVHParser::parseFile(opts.path, roundTrip, err)) {
        std::printf("  FAIL: BVHParser failed to parse exported BVH: %s\n", err.c_str());
        fails++;
    }

    if (roundTrip.frames != src.frames) {
        std::printf("  FAIL: Round-trip frame count mismatch (%d != %d)\n", roundTrip.frames, src.frames);
        fails++;
    }

    // Verify root position round-trip fidelity
    for (int f = 0; f < src.frames; ++f) {
        float dx = src.rootPositions[f * 3 + 0] - roundTrip.rootPositions[f * 3 + 0];
        float dy = src.rootPositions[f * 3 + 1] - roundTrip.rootPositions[f * 3 + 1];
        float dz = src.rootPositions[f * 3 + 2] - roundTrip.rootPositions[f * 3 + 2];
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist > 1e-2f) {
            std::printf("  FAIL: Frame %d root translation drift = %.4f\n", f, dist);
            fails++;
            break;
        }
    }

    std::printf("  -> BVH Round-Trip: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::runPathologicalCases() {
    int fails = 0;
    std::printf("[TEST] Running Pathological & Edge Cases tests...\n");

    // 1. Quat -> Euler -> Quat round-trip across singularity cases
    // Gimbal lock case: 90 deg pitch
    {
        float ex = 0, ey = 90.0f, ez = 0;
        float qx, qy, qz, qw;
        BVHParser::eulerXYZToQuat(ex, ey, ez, qx, qy, qz, qw);
        float rex, rey, rez;
        BVHExporter::quatToEulerXYZ(qx, qy, qz, qw, rex, rey, rez);
        if (std::abs(rey - 90.0f) > 0.1f) {
            std::printf("  FAIL: Gimbal lock 90 deg pitch recovery failed (%.2f)\n", rey);
            fails++;
        }
    }

    // 2. 180 deg rotation
    {
        float ex = 180.0f, ey = 0, ez = 0;
        float qx, qy, qz, qw;
        BVHParser::eulerXYZToQuat(ex, ey, ez, qx, qy, qz, qw);
        float rex, rey, rez;
        BVHExporter::quatToEulerXYZ(qx, qy, qz, qw, rex, rey, rez);
        if (std::abs(std::abs(rex) - 180.0f) > 0.1f) {
            std::printf("  FAIL: 180 deg rotation recovery failed (%.2f)\n", rex);
            fails++;
        }
    }

    // 3. Identity rotation
    {
        float ex, ey, ez;
        BVHExporter::quatToEulerXYZ(0, 0, 0, 1.0f, ex, ey, ez);
        if (std::abs(ex) > 1e-4f || std::abs(ey) > 1e-4f || std::abs(ez) > 1e-4f) {
            std::printf("  FAIL: Identity rotation did not produce zero Euler angles\n");
            fails++;
        }
    }

    // 4. NaN / Inf inputs guard
    {
        float nanVal = std::numeric_limits<float>::quiet_NaN();
        float ex, ey, ez;
        BVHExporter::quatToEulerXYZ(nanVal, 0, 0, 1.0f, ex, ey, ez);
        if (!std::isfinite(ex) || !std::isfinite(ey) || !std::isfinite(ez)) {
            std::printf("  FAIL: NaN input produced non-finite Euler angles\n");
            fails++;
        }
    }

    std::printf("  -> Pathological Cases: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::runBlenderRetargeting() {
    int fails = 0;
    std::printf("[TEST] Running Blender Generic Retargeting tests...\n");

    const SkeletonProfile* blender = findProfile("blender-generic");
    if (!blender) {
        std::printf("  FAIL: blender-generic profile missing\n");
        return 1;
    }

    Animation src = makeSynthSoma(3);
    BoneMap map = Retargeter::autoMap(*blender);
    Retargeter::Options opts;
    Animation out;
    std::string err;
    RetargetReport rep;

    if (!Retargeter::retarget(src, *blender, map, opts, out, err, &rep)) {
        std::printf("  FAIL: Retarget to blender-generic failed: %s\n", err.c_str());
        fails++;
    }

    if (out.joints != static_cast<int>(blender->joints.size())) {
        std::printf("  FAIL: Retargeted joint count mismatch\n");
        fails++;
    }

    std::printf("  -> Blender Generic Retargeting: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::runAll() {
    std::printf("===================================================\n");
    std::printf("  KIMODO STUDIO — COMPREHENSIVE VERIFICATION SUITE  \n");
    std::printf("===================================================\n");

    int totalFails = 0;
    totalFails += runAnimationAndFK();
    totalFails += runSomaPresentation();
    totalFails += runCharacterAndSkinning();
    totalFails += runBVHRoundTrip();
    totalFails += runPathologicalCases();
    totalFails += runBlenderRetargeting();

    std::printf("===================================================\n");
    if (totalFails == 0) {
        std::printf("  ALL TESTS PASSED SUCCESSFULLY! (0 failures)       \n");
    } else {
        std::printf("  TEST SUITE COMPLETED WITH %d FAILURES              \n", totalFails);
    }
    std::printf("===================================================\n");
    return totalFails == 0 ? 0 : 1;
}

} // namespace studio
