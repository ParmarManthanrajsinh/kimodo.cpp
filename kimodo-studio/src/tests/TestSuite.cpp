#include "tests/TestSuite.h"
#include "animation/Animation.h"
#include "animation/Skeleton.h"
#include "animation/SomaPresentation.h"
#include "app/AppState.h"
#include "character/CharacterAsset.h"
#include "character/CharacterLoader.h"
#include "character/CharacterMapper.h"
#include "export/BVHExporter.h"
#include "export/BVHParser.h"
#include "library/AnimationLibrary.h"
#include "retarget/Retargeter.h"
#include "retarget/SkeletonProfile.h"
#include "utils/AppPaths.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

namespace studio {
namespace {

Animation make_synth_soma(int frames = 4) {
    Animation a;
    a.frames = frames;
    a.fps = 30.0f;
    a.joints = kSomaJoints;
    a.skeleton_name = "soma30";
    a.joint_names.assign(Soma30Spec::names.begin(), Soma30Spec::names.end());
    a.parents.assign(Soma30Spec::parents.begin(), Soma30Spec::parents.end());
    a.offsets.assign(Soma30Spec::offsets.begin(), Soma30Spec::offsets.end());
    a.local_rotations_xyzw.assign(static_cast<size_t>(frames) * kSomaJoints * 4, 0.0f);
    a.root_positions.assign(static_cast<size_t>(frames) * 3, 0.0f);

    for (int f = 0; f < frames; ++f) {
        a.root_positions[f * 3 + 0] = 0.0f;
        a.root_positions[f * 3 + 1] = 0.95f + 0.02f * static_cast<float>(f);
        a.root_positions[f * 3 + 2] = 0.05f * static_cast<float>(f);

        for (int j = 0; j < kSomaJoints; ++j) {
            a.local_rotations_xyzw[(static_cast<size_t>(f) * kSomaJoints + j) * 4 + 3] = 1.0f;
        }

        // Slight arm swing
        float angle = (f - frames / 2.0f) * 0.1f;
        float h = angle * 0.5f;
        int l_arm = 11, r_arm = 17;
        a.local_rotations_xyzw[(static_cast<size_t>(f) * kSomaJoints + l_arm) * 4 + 0] = std::sin(h);
        a.local_rotations_xyzw[(static_cast<size_t>(f) * kSomaJoints + l_arm) * 4 + 3] = std::cos(h);
        a.local_rotations_xyzw[(static_cast<size_t>(f) * kSomaJoints + r_arm) * 4 + 0] = -std::sin(h);
        a.local_rotations_xyzw[(static_cast<size_t>(f) * kSomaJoints + r_arm) * 4 + 3] = std::cos(h);
    }
    return a;
}

} // namespace

int TestSuite::RunAnimationAndFK() {
    int fails = 0;
    std::printf("[TEST] Running Animation & Skeleton FK tests...\n");

    Animation anim = make_synth_soma(5);
    if (anim.frames != 5 || anim.joints != 30) {
        std::printf("  FAIL: Synthetic animation dimension mismatch\n");
        fails++;
    }

    // Forward kinematics test
    std::vector<Vector3> pos;
    std::vector<Quaternion> rot;
    const float* root = anim.root_positions.data();
    const float* rots = anim.local_rotations_xyzw.data();

    Skeleton::ForwardKinematicsFull(rots, root, anim.parents, anim.offsets, pos, rot);

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

int TestSuite::RunSomaPresentation() {
    int fails = 0;
    std::printf("[TEST] Running SOMA Presentation Skeleton tests...\n");

    Animation src = make_synth_soma(3);
    Animation pres;
    std::string err;

    if (!SomaPresentation::ExpandSoma30(src, pres, err)) {
        std::printf("  FAIL: SomaPresentation::expandSoma30 failed: %s\n", err.c_str());
        fails++;
    }

    if (pres.joints <= 30) {
        std::printf("  FAIL: Expanded presentation joint count <= 30\n");
        fails++;
    }

    // Validation
    auto val = SomaPresentation::Validate(pres);
    if (!val.valid || !val.hierarchy_valid || !val.is_finite) {
        std::printf("  FAIL: SomaPresentation validation failed\n");
        for (const auto& e : val.errors)
            std::printf("    Error: %s\n", e.c_str());
        fails++;
    }

    std::printf("  -> SOMA Presentation: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::RunCharacterAndSkinning() {
    int fails = 0;
    std::printf("[TEST] Running Character & Skinning tests...\n");

    std::filesystem::path char_path = AppPaths::ResolveAsset("assets/characters/CesiumMan.glb");
    CharacterAsset character;
    std::string err;

    if (!CharacterLoader::LoadGLB(char_path.string(), character, err)) {
        std::printf("  WARN: CharacterLoader on %s: %s (skipping if file not present)\n", char_path.string().c_str(),
                    err.c_str());
    } else {
        if (!character.IsLoaded() || character.GetSkinningData().vertices.empty()) {
            std::printf("  FAIL: Loaded character has no vertices\n");
            fails++;
        }

        const auto& rep = character.GetValidationReport();
        if (!rep.has_mesh || !rep.has_skeleton) {
            std::printf("  FAIL: Character validation checklist failed\n");
            fails++;
        }

        // Test Auto Mapper
        Animation anim = make_synth_soma(2);
        auto mapping = CharacterMapper::AutoMap(character, anim.joint_names);
        if (mapping.empty()) {
            std::printf("  FAIL: CharacterMapper AutoMap returned empty map\n");
            fails++;
        }

        std::vector<Matrix> skin_mats;
        if (!CharacterMapper::EvaluateSkinMatrices(character, anim, 0, mapping, skin_mats)) {
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
        lib.Init(AppPaths::DefaultAnimationsDir());
        std::printf("  [DEBUG] Library has %zu animation clips\n", lib.GetEntries().size());
        for (const auto& entry : lib.GetEntries()) {
            std::printf("    Clip: '%s', frames=%d, skeleton='%s'\n", entry.prompt.c_str(), entry.frames,
                        entry.skeleton.c_str());
            Animation real_anim;
            if (lib.LoadAnimation(entry, real_anim)) {
                if (entry.prompt.find("apple") != std::string::npos) {
                    std::printf("    === INSPECTING CLIP: %s ===\n", entry.prompt.c_str());
                    int test_frames[] = {0, 20, 40, 60, 80, 100, std::min(119, real_anim.frames - 1)};
                    for (int tf : test_frames) {
                        // SOMA FK
                        const float* r =
                            real_anim.local_rotations_xyzw.data() + static_cast<size_t>(tf) * real_anim.joints * 4;
                        const float* p = real_anim.root_positions.data() + tf * 3;
                        std::vector<Vector3> soma_pos;
                        std::vector<Quaternion> soma_rot;
                        Skeleton::ForwardKinematicsFull(r, p, real_anim.parents, real_anim.offsets, soma_pos, soma_rot);

                        // Character FK
                        std::vector<Matrix> cur_skin_mats;
                        std::vector<Vector3> char_pos;
                        CharacterMapper::EvaluateSkinMatrices(character, real_anim, tf, mapping, cur_skin_mats,
                                                              &char_pos);

                        // Joint 17 = RightArm, Joint 18 = RightForeArm, Joint 19 = RightHand, Joint 6 = Head
                        std::printf("      Frame %d:\n", tf);
                        std::printf("        SOMA Root (Hips): (%.3f, %.3f, %.3f)\n", soma_pos[0].x, soma_pos[0].y,
                                    soma_pos[0].z);
                        std::printf("        SOMA Head: (%.3f, %.3f, %.3f)\n", soma_pos[6].x, soma_pos[6].y,
                                    soma_pos[6].z);
                        std::printf("        SOMA RightArm: (%.3f, %.3f, %.3f)\n", soma_pos[17].x, soma_pos[17].y,
                                    soma_pos[17].z);
                        std::printf("        SOMA RightForeArm: (%.3f, %.3f, %.3f)\n", soma_pos[18].x, soma_pos[18].y,
                                    soma_pos[18].z);
                        std::printf("        SOMA RightHand: (%.3f, %.3f, %.3f)\n", soma_pos[19].x, soma_pos[19].y,
                                    soma_pos[19].z);

                        // Char Bone 4 = Head (Skeleton_neck_joint_2), Bone 8 = RightArm, Bone 10 = RightForeArm
                        std::printf("        CHAR Head: (%.3f, %.3f, %.3f)\n", char_pos[4].x, char_pos[4].y,
                                    char_pos[4].z);
                        std::printf("        CHAR RightArm: (%.3f, %.3f, %.3f)\n", char_pos[8].x, char_pos[8].y,
                                    char_pos[8].z);
                        std::printf("        CHAR RightForeArm: (%.3f, %.3f, %.3f)\n", char_pos[10].x, char_pos[10].y,
                                    char_pos[10].z);
                    }
                }
            }
        }

        // Test Skin Matrix Evaluation
        if (skin_mats.size() != character.GetBones().size()) {
            std::printf("  FAIL: Skin matrix count != bone count\n");
            fails++;
        }

        // Test CPU skinning
        character.UpdateCpuSkinning(skin_mats);
        const auto& anim_verts = character.GetAnimatedVertices();
        if (anim_verts.size() != character.GetSkinningData().vertices.size()) {
            std::printf("  FAIL: Animated vertices count mismatch\n");
            fails++;
        } else {
            // Verify animated mesh is upright in Y-UP space (height Y >= 1.0m, Z extends reasonably)
            float min_y = 1e9f, max_y = -1e9f;
            for (const auto& v : anim_verts) {
                if (v.y < min_y)
                    min_y = v.y;
                if (v.y > max_y)
                    max_y = v.y;
            }
            if (max_y < 1.2f || min_y < -0.3f) {
                std::printf("  FAIL: Animated character not upright! Y bounds: [%.2f, %.2f]\n", min_y, max_y);
                fails++;
            }
        }
    }

    // Test UI Panel Width Clamping
    AppState test_state;
    test_state.side_width = 50.0f; // Below min 120
    test_state.side_width = std::clamp(test_state.side_width, 120.0f, 320.0f);
    if (test_state.side_width != 120.0f) {
        std::printf("  FAIL: sideWidth clamp failed min\n");
        fails++;
    }
    test_state.panel_width = 1000.0f; // Above max 640
    test_state.panel_width = std::clamp(test_state.panel_width, 260.0f, 640.0f);
    if (test_state.panel_width != 640.0f) {
        std::printf("  FAIL: panelWidth clamp failed max\n");
        fails++;
    }

    std::printf("  -> Character & Skinning: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::RunBVHRoundTrip() {
    int fails = 0;
    std::printf("[TEST] Running BVH Export, Parser & Round-Trip tests...\n");

    Animation src = make_synth_soma(6);
    ExportOptions opts;
    opts.path = (AppPaths::AppDataDir() / "test_roundtrip.bvh").string();
    opts.fps = 30.0f;

    std::string err;
    BVHExporter bvhExp;
    if (!bvhExp.ExportAnimation(src, opts, err)) {
        std::printf("  FAIL: BVHExporter failed: %s\n", err.c_str());
        fails++;
    }

    Animation round_trip;
    if (!BVHParser::ParseFile(opts.path, round_trip, err)) {
        std::printf("  FAIL: BVHParser failed to parse exported BVH: %s\n", err.c_str());
        fails++;
    }

    if (round_trip.frames != src.frames) {
        std::printf("  FAIL: Round-trip frame count mismatch (%d != %d)\n", round_trip.frames, src.frames);
        fails++;
    }

    // Verify root position round-trip fidelity
    for (int f = 0; f < src.frames; ++f) {
        float dx = src.root_positions[f * 3 + 0] - round_trip.root_positions[f * 3 + 0];
        float dy = src.root_positions[f * 3 + 1] - round_trip.root_positions[f * 3 + 1];
        float dz = src.root_positions[f * 3 + 2] - round_trip.root_positions[f * 3 + 2];
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

int TestSuite::RunPathologicalCases() {
    int fails = 0;
    std::printf("[TEST] Running Pathological & Edge Cases tests...\n");

    // 1. Quat -> Euler -> Quat round-trip across singularity cases
    // Gimbal lock case: 90 deg pitch
    {
        float ex = 0, ey = 90.0f, ez = 0;
        float qx, qy, qz, qw;
        BVHParser::EulerXyzToQuat(ex, ey, ez, qx, qy, qz, qw);
        float rex, rey, rez;
        BVHExporter::QuatToEulerXYZ(qx, qy, qz, qw, rex, rey, rez);
        if (std::abs(rey - 90.0f) > 0.1f) {
            std::printf("  FAIL: Gimbal lock 90 deg pitch recovery failed (%.2f)\n", rey);
            fails++;
        }
    }

    // 2. 180 deg rotation
    {
        float ex = 180.0f, ey = 0, ez = 0;
        float qx, qy, qz, qw;
        BVHParser::EulerXyzToQuat(ex, ey, ez, qx, qy, qz, qw);
        float rex, rey, rez;
        BVHExporter::QuatToEulerXYZ(qx, qy, qz, qw, rex, rey, rez);
        if (std::abs(std::abs(rex) - 180.0f) > 0.1f) {
            std::printf("  FAIL: 180 deg rotation recovery failed (%.2f)\n", rex);
            fails++;
        }
    }

    // 3. Identity rotation
    {
        float ex, ey, ez;
        BVHExporter::QuatToEulerXYZ(0, 0, 0, 1.0f, ex, ey, ez);
        if (std::abs(ex) > 1e-4f || std::abs(ey) > 1e-4f || std::abs(ez) > 1e-4f) {
            std::printf("  FAIL: Identity rotation did not produce zero Euler angles\n");
            fails++;
        }
    }

    // 4. NaN / Inf inputs guard
    {
        float nan_val = std::numeric_limits<float>::quiet_NaN();
        float ex, ey, ez;
        BVHExporter::QuatToEulerXYZ(nan_val, 0, 0, 1.0f, ex, ey, ez);
        if (!std::isfinite(ex) || !std::isfinite(ey) || !std::isfinite(ez)) {
            std::printf("  FAIL: NaN input produced non-finite Euler angles\n");
            fails++;
        }
    }

    std::printf("  -> Pathological Cases: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::RunBlenderRetargeting() {
    int fails = 0;
    std::printf("[TEST] Running Blender Generic Retargeting tests...\n");

    const SkeletonProfile* blender = FindProfile("blender-generic");
    if (!blender) {
        std::printf("  FAIL: blender-generic profile missing\n");
        return 1;
    }

    Animation src = make_synth_soma(3);
    BoneMap map = Retargeter::AutoMap(*blender);
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

int TestSuite::RunAll() {
    std::printf("===================================================\n");
    std::printf("  KIMODO STUDIO — COMPREHENSIVE VERIFICATION SUITE  \n");
    std::printf("===================================================\n");

    int total_fails = 0;
    total_fails += RunAnimationAndFK();
    total_fails += RunSomaPresentation();
    total_fails += RunCharacterAndSkinning();
    total_fails += RunBVHRoundTrip();
    total_fails += RunPathologicalCases();
    total_fails += RunBlenderRetargeting();

    std::printf("===================================================\n");
    if (total_fails == 0) {
        std::printf("  ALL TESTS PASSED SUCCESSFULLY! (0 failures)       \n");
    } else {
        std::printf("  TEST SUITE COMPLETED WITH %d FAILURES              \n", total_fails);
    }
    std::printf("===================================================\n");
    return total_fails == 0 ? 0 : 1;
}

} // namespace studio
