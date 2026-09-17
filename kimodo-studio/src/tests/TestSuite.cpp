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
#include "raymath.h"
#include "retarget/Retargeter.h"
#include "retarget/SkeletonProfile.h"
#include "utils/AppPaths.h"

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

        std::printf("  [DEBUG] Character has %zu bones, %zu vertices\n", character.bones().size(), character.skinningData().vertices.size());
        for (size_t b = 0; b < character.bones().size(); ++b) {
            const auto& bn = character.bones()[b];
            std::printf("    Bone %zu: '%s', parent=%d, restPos=(%.2f, %.2f, %.2f), map='%s'\n",
                        b, bn.name.c_str(), bn.parent, bn.restPosition.x, bn.restPosition.y, bn.restPosition.z,
                        mapping.count(bn.name) ? mapping[bn.name].c_str() : "none");
        }

        // Test Skin Matrix Evaluation
        std::vector<Matrix> skinMats;
        if (!CharacterMapper::evaluateSkinMatrices(character, anim, 0, mapping, skinMats)) {
            std::printf("  FAIL: evaluateSkinMatrices returned false\n");
            fails++;
        }

        if (skinMats.size() != character.bones().size()) {
            std::printf("  FAIL: Skin matrix count != bone count\n");
            fails++;
        }

        // Test CPU skinning
        character.updateCpuSkinning(skinMats);
        if (character.animatedVertices().size() != character.skinningData().vertices.size()) {
            std::printf("  FAIL: Animated vertices count mismatch\n");
            fails++;
        }
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
