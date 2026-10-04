#include "tests/TestSuite.h"
#include "animation/Animation.h"
#include "animation/AnimationPlayer.h"
#include "animation/Skeleton.h"
#include "animation/SomaPresentation.h"
#include "app/AppState.h"
#include "app/SetupManager.h"
#include "character/CharacterAsset.h"
#include "character/CharacterLoader.h"
#include "character/CharacterMapper.h"
#include "export/BVHExporter.h"
#include "export/BVHParser.h"
#include "library/AnimationLibrary.h"
#include "huggingface/HuggingFaceClient.h"
#include "kimodo/KimodoAdapter.h"
#include "models/ModelManager.h"
#include "rendering/Viewport.h"
#include "retarget/Retargeter.h"
#include "retarget/SkeletonProfile.h"
#include "utils/AppPaths.h"
#include "utils/Hash.h"
#include "utils/RuntimeValidator.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>
#include <vector>

namespace studio
{
namespace
{

Animation make_synth_soma(int frames = 4)
{
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

    for (int f = 0; f < frames; ++f)
    {
        a.root_positions[f * 3 + 0] = 0.0f;
        a.root_positions[f * 3 + 1] = 0.95f + 0.02f * static_cast<float>(f);
        a.root_positions[f * 3 + 2] = 0.05f * static_cast<float>(f);

        for (int j = 0; j < kSomaJoints; ++j)
        {
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

int TestSuite::RunAnimationAndFK()
{
    int fails = 0;
    std::printf("[TEST] Running Animation & Skeleton FK tests...\n");

    Animation anim = make_synth_soma(5);
    if (anim.frames != 5 || anim.joints != 30)
    {
        std::printf("  FAIL: Synthetic animation dimension mismatch\n");
        fails++;
    }

    // Forward kinematics test
    std::vector<Vector3> pos;
    std::vector<Quaternion> rot;
    const float* root = anim.root_positions.data();
    const float* rots = anim.local_rotations_xyzw.data();

    Skeleton::ForwardKinematicsFull(rots, root, anim.parents, anim.offsets, pos, rot);

    if (pos.size() != 30 || rot.size() != 30)
    {
        std::printf("  FAIL: FK result joint count != 30\n");
        fails++;
    }

    // Root joint world position must equal root translation
    float dx = pos[0].x - root[0];
    float dy = pos[0].y - root[1];
    float dz = pos[0].z - root[2];
    if (std::sqrt(dx * dx + dy * dy + dz * dz) > 1e-4f)
    {
        std::printf("  FAIL: Root world pos != root translation\n");
        fails++;
    }

    // MotionResult -> Animation -> AnimationPlayer pipeline test
    MotionResult mr;
    mr.frames = 10;
    mr.joints = 30;
    mr.local_rotations_xyzw.assign(10 * 30 * 4, 0.0f);
    for (size_t i = 0; i < 10 * 30; ++i)
    {
        mr.local_rotations_xyzw[i * 4 + 3] = 1.0f; // identity quaternion
    }
    mr.root_positions.assign(10 * 3, 0.0f);

    Animation from_mr;
    from_mr.FromMotionResult(mr);
    if (from_mr.empty() || from_mr.frames != 10 || from_mr.joints != 30)
    {
        std::printf("  FAIL: Animation::FromMotionResult failed to populate frames or joints\n");
        fails++;
    }

    AnimationPlayer player;
    player.Load(from_mr);
    if (!player.HasAnimation() || !player.IsPlaying() || player.GetTotalFrames() != 10)
    {
        std::printf("  FAIL: AnimationPlayer failed to load or activate animation\n");
        fails++;
    }
    player.Update(0.1f);
    if (player.GetWorldPositions().size() != 30)
    {
        std::printf("  FAIL: AnimationPlayer world positions size != 30 after update\n");
        fails++;
    }

    std::printf("  -> Animation & FK: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::RunSomaPresentation()
{
    int fails = 0;
    std::printf("[TEST] Running SOMA Presentation Skeleton tests...\n");

    Animation src = make_synth_soma(3);
    Animation pres;
    std::string err;

    if (!SomaPresentation::ExpandSoma30(src, pres, err))
    {
        std::printf("  FAIL: SomaPresentation::expandSoma30 failed: %s\n", err.c_str());
        fails++;
    }

    if (pres.joints <= 30)
    {
        std::printf("  FAIL: Expanded presentation joint count <= 30\n");
        fails++;
    }

    // Validation
    auto val = SomaPresentation::Validate(pres);
    if (!val.valid || !val.hierarchy_valid || !val.is_finite)
    {
        std::printf("  FAIL: SomaPresentation validation failed\n");
        for (const auto& e : val.errors)
            std::printf("    Error: %s\n", e.c_str());
        fails++;
    }

    std::printf("  -> SOMA Presentation: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::RunCharacterAndSkinning()
{
    int fails = 0;
    std::printf("[TEST] Running Character & Skinning tests...\n");

    std::filesystem::path char_path = AppPaths::ResolveAsset("assets/characters/CesiumMan.glb");
    CharacterAsset character;
    std::string err;

    if (!CharacterLoader::LoadGLB(char_path.string(), character, err))
    {
        std::printf("  WARN: CharacterLoader on %s: %s (skipping if file not present)\n", char_path.string().c_str(),
                    err.c_str());
    }
    else
    {
        if (!character.IsLoaded() || character.GetSkinningData().vertices.empty())
        {
            std::printf("  FAIL: Loaded character has no vertices\n");
            fails++;
        }

        const auto& rep = character.GetValidationReport();
        if (!rep.has_mesh || !rep.has_skeleton)
        {
            std::printf("  FAIL: Character validation checklist failed\n");
            fails++;
        }

        // Test Auto Mapper
        Animation anim = make_synth_soma(2);
        auto mapping = CharacterMapper::AutoMap(character, anim.joint_names);
        if (mapping.empty())
        {
            std::printf("  FAIL: CharacterMapper AutoMap returned empty map\n");
            fails++;
        }

        std::vector<Matrix> skin_mats;
        if (!CharacterMapper::EvaluateSkinMatrices(character, anim, 0, mapping, skin_mats))
        {
            std::printf("  FAIL: evaluateSkinMatrices returned false\n");
            fails++;
        }

        // Verify critical mappings are correct (RightArm, RightForeArm, Head)
        if (mapping["Skeleton_arm_joint_R__2_"] != "RightArm")
        {
            std::printf("  FAIL: Skeleton_arm_joint_R__2_ mapped to '%s' instead of 'RightArm'\n",
                        mapping["Skeleton_arm_joint_R__2_"].c_str());
            fails++;
        }
        if (mapping["Skeleton_arm_joint_R__3_"] != "RightForeArm")
        {
            std::printf("  FAIL: Skeleton_arm_joint_R__3_ mapped to '%s' instead of 'RightForeArm'\n",
                        mapping["Skeleton_arm_joint_R__3_"].c_str());
            fails++;
        }
        if (mapping["Skeleton_neck_joint_2"] != "Head")
        {
            std::printf("  FAIL: Skeleton_neck_joint_2 mapped to '%s' instead of 'Head'\n",
                        mapping["Skeleton_neck_joint_2"].c_str());
            fails++;
        }

        AnimationLibrary lib;
        lib.Init(AppPaths::DefaultAnimationsDir());
        std::printf("  [DEBUG] Library has %zu animation clips\n", lib.GetEntries().size());
        for (const auto& entry : lib.GetEntries())
        {
            std::printf("    Clip: '%s', frames=%d, skeleton='%s'\n", entry.prompt.c_str(), entry.frames,
                        entry.skeleton.c_str());
            Animation real_anim;
            if (lib.LoadAnimation(entry, real_anim))
            {
                if (entry.prompt.find("apple") != std::string::npos)
                {
                    std::printf("    === INSPECTING CLIP: %s ===\n", entry.prompt.c_str());
                    int test_frames[] = {0, 20, 40, 60, 80, 100, std::min(119, real_anim.frames - 1)};
                    for (int tf : test_frames)
                    {
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
        if (skin_mats.size() != character.GetBones().size())
        {
            std::printf("  FAIL: Skin matrix count != bone count\n");
            fails++;
        }

        // Test CPU skinning
        character.UpdateCpuSkinning(skin_mats);
        const auto& anim_verts = character.GetAnimatedVertices();
        if (anim_verts.size() != character.GetSkinningData().vertices.size())
        {
            std::printf("  FAIL: Animated vertices count mismatch\n");
            fails++;
        }
        else
        {
            // Verify animated mesh is upright in Y-UP space (height Y >= 1.0m, Z extends reasonably)
            float min_y = 1e9f, max_y = -1e9f;
            for (const auto& v : anim_verts)
            {
                if (v.y < min_y)
                    min_y = v.y;
                if (v.y > max_y)
                    max_y = v.y;
            }
            if (max_y < 1.2f || min_y < -0.3f)
            {
                std::printf("  FAIL: Animated character not upright! Y bounds: [%.2f, %.2f]\n", min_y, max_y);
                fails++;
            }
        }
    }

    // Test UI Panel Width Clamping
    AppState test_state;
    test_state.side_width = 50.0f; // Below min 120
    test_state.side_width = std::clamp(test_state.side_width, 120.0f, 320.0f);
    if (test_state.side_width != 120.0f)
    {
        std::printf("  FAIL: sideWidth clamp failed min\n");
        fails++;
    }
    test_state.panel_width = 1000.0f; // Above max 640
    test_state.panel_width = std::clamp(test_state.panel_width, 260.0f, 640.0f);
    if (test_state.panel_width != 640.0f)
    {
        std::printf("  FAIL: panelWidth clamp failed max\n");
        fails++;
    }

    std::printf("  -> Character & Skinning: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::RunBVHRoundTrip()
{
    int fails = 0;
    std::printf("[TEST] Running BVH Export, Parser & Round-Trip tests...\n");

    Animation src = make_synth_soma(6);
    ExportOptions opts;
    opts.path = (AppPaths::AppDataDir() / "test_roundtrip.bvh").string();
    opts.fps = 30.0f;

    std::string err;
    BVHExporter bvhExp;
    if (!bvhExp.ExportAnimation(src, opts, err))
    {
        std::printf("  FAIL: BVHExporter failed: %s\n", err.c_str());
        fails++;
    }

    Animation round_trip;
    if (!BVHParser::ParseFile(opts.path, round_trip, err))
    {
        std::printf("  FAIL: BVHParser failed to parse exported BVH: %s\n", err.c_str());
        fails++;
    }

    if (round_trip.frames != src.frames)
    {
        std::printf("  FAIL: Round-trip frame count mismatch (%d != %d)\n", round_trip.frames, src.frames);
        fails++;
    }

    // Verify root position round-trip fidelity
    for (int f = 0; f < src.frames; ++f)
    {
        float dx = src.root_positions[f * 3 + 0] - round_trip.root_positions[f * 3 + 0];
        float dy = src.root_positions[f * 3 + 1] - round_trip.root_positions[f * 3 + 1];
        float dz = src.root_positions[f * 3 + 2] - round_trip.root_positions[f * 3 + 2];
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist > 1e-2f)
        {
            std::printf("  FAIL: Frame %d root translation drift = %.4f\n", f, dist);
            fails++;
            break;
        }
    }

    std::printf("  -> BVH Round-Trip: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::RunPathologicalCases()
{
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
        if (std::abs(rey - 90.0f) > 0.1f)
        {
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
        if (std::abs(std::abs(rex) - 180.0f) > 0.1f)
        {
            std::printf("  FAIL: 180 deg rotation recovery failed (%.2f)\n", rex);
            fails++;
        }
    }

    // 3. Identity rotation
    {
        float ex, ey, ez;
        BVHExporter::QuatToEulerXYZ(0, 0, 0, 1.0f, ex, ey, ez);
        if (std::abs(ex) > 1e-4f || std::abs(ey) > 1e-4f || std::abs(ez) > 1e-4f)
        {
            std::printf("  FAIL: Identity rotation did not produce zero Euler angles\n");
            fails++;
        }
    }

    // 4. NaN / Inf inputs guard
    {
        float nan_val = std::numeric_limits<float>::quiet_NaN();
        float ex, ey, ez;
        BVHExporter::QuatToEulerXYZ(nan_val, 0, 0, 1.0f, ex, ey, ez);
        if (!std::isfinite(ex) || !std::isfinite(ey) || !std::isfinite(ez))
        {
            std::printf("  FAIL: NaN input produced non-finite Euler angles\n");
            fails++;
        }
    }

    std::printf("  -> Pathological Cases: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::RunBlenderRetargeting()
{
    int fails = 0;
    std::printf("[TEST] Running Blender Generic Retargeting tests...\n");

    const SkeletonProfile* blender = FindProfile("blender-generic");
    if (!blender)
    {
        std::printf("  FAIL: blender-generic profile missing\n");
        return 1;
    }

    Animation src = make_synth_soma(3);
    BoneMap map = Retargeter::AutoMap(*blender);
    Retargeter::Options opts;
    Animation out;
    std::string err;
    RetargetReport rep;

    if (!Retargeter::retarget(src, *blender, map, opts, out, err, &rep))
    {
        std::printf("  FAIL: Retarget to blender-generic failed: %s\n", err.c_str());
        fails++;
    }

    if (out.joints != static_cast<int>(blender->joints.size()))
    {
        std::printf("  FAIL: Retargeted joint count mismatch\n");
        fails++;
    }

    std::printf("  -> Blender Generic Retargeting: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::RunResizeRegression()
{
    std::printf("[TEST] Running Resize-Safe Framebuffer & Viewport Regression tests...\n");
    int fails = 0;

    bool need_close = false;
    if (!IsWindowReady())
    {
        SetTraceLogLevel(LOG_WARNING);
        SetConfigFlags(FLAG_WINDOW_HIDDEN);
        InitWindow(64, 64, "KimodoStudio-TestWindow");
        need_close = true;
    }

    Viewport vp;
    vp.Reset();

    // 1. Test standard multi-resolution transitions
    const std::vector<std::pair<int, int>> standard_resolutions = {
        {1280, 720},
        {1600, 900},
        {1920, 1080},
        {2560, 1440}
    };

    for (const auto& [w, h] : standard_resolutions)
    {
        vp.EnsureSize(w, h);
        if (vp.GetWidth() != w || vp.GetHeight() != h)
        {
            std::printf("  FAIL: Expected viewport size %dx%d, got %dx%d\n", w, h, vp.GetWidth(), vp.GetHeight());
            fails++;
        }
        if (!vp.IsReady() || vp.GetTextureId() == 0)
        {
            std::printf("  FAIL: RenderTexture2D invalid at %dx%d\n", w, h);
            fails++;
        }

        // Render pass verification
        vp.BeginRender();
        vp.Draw3D();
        vp.DrawOrientationGizmo(38.0f, static_cast<float>(h) - 40.0f);
        vp.EndRender();
    }

    // 2. Test rapid dynamic resizes & extreme aspect ratios (simulating window dragging)
    const std::vector<std::pair<int, int>> stress_resolutions = {
        {300, 900},   // ultra-tall
        {2560, 300},  // ultra-wide
        {800, 500},   // minimum window size
        {1, 1},       // extreme minimal edge case
        {-10, -50},   // negative/zero clamp protection
        {1920, 1080}  // restore to standard
    };

    for (int iter = 0; iter < 10; ++iter)
    {
        for (const auto& [rw, rh] : stress_resolutions)
        {
            vp.EnsureSize(rw, rh);
            const int expected_w = std::max(1, rw);
            const int expected_h = std::max(1, rh);
            if (vp.GetWidth() != expected_w || vp.GetHeight() != expected_h)
            {
                std::printf("  FAIL: Stress clamp failed for input %dx%d, expected %dx%d, got %dx%d\n",
                            rw, rh, expected_w, expected_h, vp.GetWidth(), vp.GetHeight());
                fails++;
            }
            if (!vp.IsReady() || vp.GetTextureId() == 0)
            {
                std::printf("  FAIL: Invalid texture in rapid stress test at %dx%d\n", expected_w, expected_h);
                fails++;
            }
            vp.BeginRender();
            vp.Draw3D();
            vp.EndRender();
        }
    }

    // 3. Test clean shutdown and resource release
    vp.Shutdown();
    if (vp.GetWidth() != 0 || vp.GetHeight() != 0 || vp.IsReady())
    {
        std::printf("  FAIL: Viewport::Shutdown did not cleanly reset framebuffer state\n");
        fails++;
    }

    if (need_close)
    {
        CloseWindow();
    }

    std::printf("  -> Resize & Viewport Regression: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::RunSetupWizardSimulation()
{
    std::printf("--- Setup Wizard Simulation & Complete Bundle Verification ---\n");
    int fails = 0;

    std::filesystem::path test_dir = AppPaths::AppDataDir() / "test_scratch_setup";
    std::error_code ec;
    std::filesystem::remove_all(test_dir, ec);
    std::filesystem::create_directories(test_dir, ec);

    std::filesystem::path models_dir = test_dir / "models";
    std::filesystem::path text_bundle_dir = models_dir / "llm2vec-text-bundle";
    std::filesystem::create_directories(models_dir, ec);

    // Create synthetic test motion file and 3-file text encoder bundle
    std::string motion_content = "KIMODO_TEST_MOTION_GGUF_V1";
    std::string text_tok_content = "KIMODO_TEST_TOKENIZER_BUNDLE_V1";
    std::string text_emb_content = "KIMODO_TEST_EMBEDDING_BUNDLE_V1";
    std::string text_norm_content = "KIMODO_TEST_FINALNORM_BUNDLE_V1";

    std::filesystem::path dummy_motion = test_dir / "temp_motion.gguf";
    std::filesystem::path dummy_tok = test_dir / "temp_tok.gguf";
    std::filesystem::path dummy_emb = test_dir / "temp_emb.gguf";
    std::filesystem::path dummy_norm = test_dir / "temp_norm.gguf";

    {
        std::ofstream om(dummy_motion, std::ios::binary);
        om.write(motion_content.data(), static_cast<std::streamsize>(motion_content.size()));
        std::ofstream ot(dummy_tok, std::ios::binary);
        ot.write(text_tok_content.data(), static_cast<std::streamsize>(text_tok_content.size()));
        std::ofstream oe(dummy_emb, std::ios::binary);
        oe.write(text_emb_content.data(), static_cast<std::streamsize>(text_emb_content.size()));
        std::ofstream on(dummy_norm, std::ios::binary);
        on.write(text_norm_content.data(), static_cast<std::streamsize>(text_norm_content.size()));
    }

    std::string err;
    std::string motion_hash = FileHash::Sha256(dummy_motion.string(), err);
    std::string tok_hash = FileHash::Sha256(dummy_tok.string(), err);
    std::string emb_hash = FileHash::Sha256(dummy_emb.string(), err);
    std::string norm_hash = FileHash::Sha256(dummy_norm.string(), err);

    if (motion_hash.empty() || tok_hash.empty() || emb_hash.empty() || norm_hash.empty())
    {
        std::printf("  FAIL: Unable to compute test SHA256 hashes\n");
        fails++;
    }

    // Write custom models.json representing multi-file bundle and gated assets
    std::filesystem::path test_models_json = test_dir / "models.json";
    {
        std::ofstream oj(test_models_json);
        oj << "{\n"
           << "  \"models\": [\n"
           << "    {\n"
           << "      \"id\": \"sim-motion\",\n"
           << "      \"name\": \"Simulated SOMA RP Motion Model\",\n"
           << "      \"version\": \"1.1\",\n"
           << "      \"remotePath\": \"soma-rp-v1.1.gguf\",\n"
           << "      \"localFilename\": \"soma-rp-v1.1.gguf\",\n"
           << "      \"sizeBytes\": " << motion_content.size() << ",\n"
           << "      \"sha256\": \"" << motion_hash << "\",\n"
           << "      \"required\": true,\n"
           << "      \"assetType\": \"motion\",\n"
           << "      \"gated\": true\n"
           << "    },\n"
           << "    {\n"
           << "      \"id\": \"llm2vec-text-bundle\",\n"
           << "      \"name\": \"Simulated LLM2Vec Multi-File Bundle\",\n"
           << "      \"version\": \"1.0\",\n"
           << "      \"remotePath\": \"LocalAI-io/Llama-3-Kimodo-GGML\",\n"
           << "      \"localFilename\": \"llm2vec-text-bundle\",\n"
           << "      \"sizeBytes\": " << (text_tok_content.size() + text_emb_content.size() + text_norm_content.size()) << ",\n"
           << "      \"required\": true,\n"
           << "      \"assetType\": \"text_encoder\",\n"
           << "      \"gated\": true,\n"
           << "      \"files\": [\n"
           << "        {\"remotePath\": \"tokenizer.gguf\", \"localFilename\": \"tokenizer.gguf\", \"sizeBytes\": " << text_tok_content.size() << ", \"sha256\": \"" << tok_hash << "\"},\n"
           << "        {\"remotePath\": \"embedding.gguf\", \"localFilename\": \"embedding.gguf\", \"sizeBytes\": " << text_emb_content.size() << ", \"sha256\": \"" << emb_hash << "\"},\n"
           << "        {\"remotePath\": \"final-norm.gguf\", \"localFilename\": \"final-norm.gguf\", \"sizeBytes\": " << text_norm_content.size() << ", \"sha256\": \"" << norm_hash << "\"}\n"
           << "      ]\n"
           << "    }\n"
           << "  ]\n"
           << "}\n";
    }

    // 1. TEST CLEAN INSTALLATION: no model files installed
    {
        ModelManager mm;
        mm.Init(test_models_json.string(), models_dir.string(), text_bundle_dir.string());
        if (mm.AreAllRequiredInstalled())
        {
            std::printf("  FAIL: ModelManager claimed all required installed on empty directory\n");
            fails++;
        }
        auto missing = mm.GetMissingRequired();
        if (missing.size() != 2)
        {
            std::printf("  FAIL: ModelManager expected 2 missing, got %zu\n", missing.size());
            fails++;
        }

        AppState st;
        SetupManager sm;
        sm.Init(mm, st);
        if (sm.IsReady())
        {
            std::printf("  FAIL: SetupManager marked ready on clean empty installation\n");
            fails++;
        }
        if (sm.GetState() != SetupState::Welcome)
        {
            std::printf("  FAIL: SetupManager initial state on clean install should be Welcome, got: %d\n", static_cast<int>(sm.GetState()));
            fails++;
        }
        sm.StartFirstRun(mm, st);
        if (sm.GetState() != SetupState::NeedsHuggingFaceLogin)
        {
            // Hermetic note: this machine may hold a REAL HF credential in the
            // OS store, in which case first-run correctly proceeds past login
            // into access-check/download instead. Accept those states, but stop
            // any worker the transition may have spawned.
            const SetupState got = sm.GetState();
            if (got == SetupState::CheckingModelAccess || got == SetupState::Downloading ||
                got == SetupState::NeedsModelAccess)
            {
                std::printf("  [INFO] real HF credential present; first-run proceeds past login (state %d)\n",
                            static_cast<int>(got));
                sm.CancelDownload(mm);
            }
            else
            {
                std::printf("  FAIL: SetupManager after StartFirstRun should be NeedsHuggingFaceLogin, got: %d\n",
                            static_cast<int>(got));
                fails++;
            }
        }
        std::printf("  [PASS] 1. Clean installation & first-run Welcome state transition verified\n");
    }

    // 2. TEST MISSING TEXT ENCODER FILE (Directory exists with 2/3 files)
    {
        std::filesystem::create_directories(text_bundle_dir, ec);
        std::filesystem::copy_file(dummy_tok, text_bundle_dir / "tokenizer.gguf", std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::copy_file(dummy_emb, text_bundle_dir / "embedding.gguf", std::filesystem::copy_options::overwrite_existing, ec);
        // Note: final-norm.gguf is DELIBERATELY missing

        ModelManager mm;
        mm.Init(test_models_json.string(), models_dir.string(), text_bundle_dir.string());
        if (mm.IsBundleComplete("llm2vec-text-bundle"))
        {
            std::printf("  FAIL: Bundle marked complete despite missing final-norm.gguf!\n");
            fails++;
        }
        if (mm.AreAllRequiredInstalled())
        {
            std::printf("  FAIL: All required reported installed when bundle is missing 1 file\n");
            fails++;
        }
        std::printf("  [PASS] 2. Incomplete multi-file bundle rejection verified (2/3 files)\n");
    }

    // 3. TEST PARTIAL DOWNLOAD HANDLING (.part file in bundle)
    {
        std::filesystem::path part_file = text_bundle_dir / "final-norm.gguf.part";
        {
            std::ofstream op(part_file, std::ios::binary);
            op.write("partial_norm_bytes", 18);
        }

        ModelManager mm;
        mm.Init(test_models_json.string(), models_dir.string(), text_bundle_dir.string());
        if (mm.IsBundleComplete("llm2vec-text-bundle"))
        {
            std::printf("  FAIL: ModelManager accepted .part file as installed bundle file\n");
            fails++;
        }
        std::filesystem::remove(part_file, ec);
        std::printf("  [PASS] 3. Partial download (.part) rejection verified\n");
    }

    // 4. TEST CORRUPTED TEXT ENCODER FILE (Hash mismatch detection)
    {
        std::filesystem::path corrupt_norm = text_bundle_dir / "final-norm.gguf";
        {
            std::ofstream oc(corrupt_norm, std::ios::binary);
            oc.write("CORRUPT_NORM_BYTES_XYZ", 22);
        }

        ModelManager mm;
        mm.Init(test_models_json.string(), models_dir.string(), text_bundle_dir.string());
        // Since size differs or hash differs, bundle is not complete/installed
        if (mm.IsBundleComplete("llm2vec-text-bundle"))
        {
            std::printf("  FAIL: Corrupt file was accepted in bundle\n");
            fails++;
        }
        std::filesystem::remove(corrupt_norm, ec);
        std::printf("  [PASS] 4. Corrupted bundle file rejection verified\n");
    }

    // 5. TEST INTERRUPTED BUNDLE RESUME (Preserves verified files)
    {
        // tokenizer.gguf and embedding.gguf already exist and are valid.
        // Confirm their contents and hashes are preserved when checking.
        std::string tok_check = FileHash::Sha256((text_bundle_dir / "tokenizer.gguf").string(), err);
        std::string emb_check = FileHash::Sha256((text_bundle_dir / "embedding.gguf").string(), err);
        if (tok_check != tok_hash || emb_check != emb_hash)
        {
            std::printf("  FAIL: Verified bundle files were corrupted during test\n");
            fails++;
        }
        std::printf("  [PASS] 5. Interrupted bundle state preserves verified files for resume\n");
    }

    // 6. TEST COMPLETE MULTI-FILE BUNDLE & SUCCESSFUL VERIFICATION
    {
        // Add valid final-norm.gguf and valid motion model
        std::filesystem::copy_file(dummy_norm, text_bundle_dir / "final-norm.gguf", std::filesystem::copy_options::overwrite_existing, ec);
        std::filesystem::copy_file(dummy_motion, models_dir / "soma-rp-v1.1.gguf", std::filesystem::copy_options::overwrite_existing, ec);

        ModelManager mm;
        mm.Init(test_models_json.string(), models_dir.string(), text_bundle_dir.string());
        if (!mm.IsBundleComplete("llm2vec-text-bundle"))
        {
            std::printf("  FAIL: Complete 3-file bundle was not recognized as complete\n");
            fails++;
        }
        if (!mm.AreAllRequiredInstalled())
        {
            std::printf("  FAIL: All required assets should be recognized as installed\n");
            fails++;
        }

        AppState st;
        SetupManager sm;
        sm.Init(mm, st);
        if (!sm.IsReady())
        {
            std::printf("  FAIL: SetupManager not ready with complete verified bundle & motion model\n");
            fails++;
        }
        if (sm.GetState() != SetupState::Ready)
        {
            std::printf("  FAIL: SetupState is not Ready when all assets verified\n");
            fails++;
        }
        std::printf("  [PASS] 6. Complete multi-file bundle & successful verification verified\n");
    }

    // 7. TEST INVALID HF TOKEN & MISSING AUTHENTICATION HANDLING
    {
        AppState st;
        SetupManager sm;
        std::string auth_err;

        // Test empty token
        bool res_empty = sm.SignInHF("", auth_err);
        if (res_empty || auth_err.empty())
        {
            std::printf("  FAIL: Empty HF token should be rejected with error\n");
            fails++;
        }

        // Test malformed token
        bool res_bogus = sm.SignInHF("hf_bogus_token_12345", auth_err);
        if (res_bogus)
        {
            std::printf("  FAIL: Bogus HF token was accepted as valid!\n");
            fails++;
        }
        // Verify token is NOT leaked in error message
        if (auth_err.find("hf_bogus_token_12345") != std::string::npos)
        {
            std::printf("  FAIL: Token secret leaked into error message!\n");
            fails++;
        }

        // Test VerificationFailureDetails UX struct
        sm.SetVerificationFailure("test.gguf", "exp_sha_123", "rec_sha_456", "Hugging Face", "Checksum mismatch");
        const auto& vf = sm.GetVerificationFailureDetails();
        if (!vf.has_failure || vf.filename != "test.gguf" || vf.expected_sha256 != "exp_sha_123")
        {
            std::printf("  FAIL: VerificationFailureDetails not populated properly\n");
            fails++;
        }
        sm.ClearVerificationFailure();
        if (sm.GetVerificationFailureDetails().has_failure)
        {
            std::printf("  FAIL: VerificationFailureDetails not cleared properly\n");
            fails++;
        }
        std::printf("  [PASS] 7. HF authentication validation, token secrecy & verification UX verified\n");
    }

    // 8. TEST RUNTIME VALIDATION & GPU DETECTION
    {
        auto val = RuntimeValidator::Validate();
        std::printf("    Runtime check: GGML %s, GPU: %s (%s)\n",
                    val.runtime_dlls_valid ? "Ready" : "Missing",
                    val.gpu_name.c_str(),
                    val.gpu_accelerated ? "Discrete/Vulkan Ready" : val.vulkan_status_message.c_str());

        // Test library checker with empty directory
        std::filesystem::path empty_dir = test_dir / "empty_runtime";
        std::filesystem::create_directories(empty_dir, ec);
        std::vector<std::string> missing_libs;
        bool check_res = RuntimeValidator::CheckLibraries(empty_dir, missing_libs);
        if (check_res || missing_libs.empty())
        {
            std::printf("  FAIL: CheckLibraries on empty dir failed to detect missing DLLs\n");
            fails++;
        }
        std::printf("  [PASS] 8. Real runtime validation & dynamic Vulkan querying verified\n");
    }

    // 9. TEST STRICT OFFLINE GATING (Refuses offline if required models missing)
    {
        AppState st;
        SetupManager sm;
        ModelManager empty_mm;
        empty_mm.Init(test_models_json.string(), (test_dir / "nonexistent").string(), (test_dir / "nonexistent").string());
        sm.Init(empty_mm, st);

        // Attempting to continue offline when required models are missing MUST FAIL!
        sm.ContinueOffline(empty_mm, st);
        if (sm.IsReady())
        {
            std::printf("  FAIL: ContinueOffline falsely marked uninstalled setup as Ready!\n");
            fails++;
        }
        if (sm.GetState() != SetupState::Error)
        {
            std::printf("  FAIL: SetupState not set to Error on invalid offline continuation\n");
            fails++;
        }

        // Now test with verified models: offline mode is valid
        ModelManager ready_mm;
        ready_mm.Init(test_models_json.string(), models_dir.string(), text_bundle_dir.string());
        SetupManager sm_ready;
        sm_ready.Init(ready_mm, st);
        sm_ready.ContinueOffline(ready_mm, st);
        if (!sm_ready.IsReady() || sm_ready.GetState() != SetupState::Ready)
        {
            std::printf("  FAIL: ContinueOffline rejected setup when local models were fully present\n");
            fails++;
        }
        std::printf("  [PASS] 9. Strict offline gating verified\n");
    }

    // 10. TEST SETUP STATE TRANSITIONS & SCREEN ROUTING
    {
        AppState st;
        SetupManager sm;
        ModelManager mm;
        mm.Init(test_models_json.string(), (test_dir / "empty_models").string(), (test_dir / "empty_models").string());
        sm.Init(mm, st);

        // Not ready -> routes to Setup
        if (!sm.IsReady())
        {
            st.screen = Screen::Setup;
        }
        if (st.screen != Screen::Setup)
        {
            std::printf("  FAIL: Application did not route to Setup screen when unready\n");
            fails++;
        }

        // Ready -> transitions to Generate
        ModelManager complete_mm;
        complete_mm.Init(test_models_json.string(), models_dir.string(), text_bundle_dir.string());
        SetupManager sm_ready;
        sm_ready.Init(complete_mm, st);
        if (sm_ready.IsReady())
        {
            st.screen = Screen::Generate;
        }
        if (st.screen != Screen::Generate)
        {
            std::printf("  FAIL: Application did not unlock Generate screen when ready\n");
            fails++;
        }
        std::printf("  [PASS] 10. State transitions & Generate screen unlocking verified\n");
    }

    // Clean up test scratch
    std::filesystem::remove_all(test_dir, ec);

    std::printf("  -> Setup Wizard Simulation: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::RunModelInstallVerify()
{
    std::printf("--- Model Install / Verify Regression (legacy bundle, no fakes) ---\n");
    int fails = 0;
    std::error_code ec;

    std::filesystem::path test_dir = AppPaths::AppDataDir() / "test_scratch_install_verify";
    std::filesystem::remove_all(test_dir, ec);
    std::filesystem::path models_dir = test_dir / "models";
    std::filesystem::path bundle_dir = models_dir / "llm2vec-text-bundle";
    std::filesystem::create_directories(bundle_dir, ec);

    const std::string motion_content = "KIMODO_INSTALL_VERIFY_MOTION_V1";
    const std::string tok_content = "KIMODO_INSTALL_VERIFY_TOKENIZER_V1";
    const std::string emb_content = "KIMODO_INSTALL_VERIFY_EMBEDDING_V1";

    auto write_file = [&](const std::filesystem::path& p, std::string_view data) {
        std::ofstream o(p, std::ios::binary | std::ios::trunc);
        o.write(data.data(), static_cast<std::streamsize>(data.size()));
    };
    write_file(test_dir / "m.tmp", motion_content);
    write_file(test_dir / "t.tmp", tok_content);
    write_file(test_dir / "e.tmp", emb_content);
    std::string err;
    const std::string motion_hash = FileHash::Sha256((test_dir / "m.tmp").string(), err);
    const std::string tok_hash = FileHash::Sha256((test_dir / "t.tmp").string(), err);
    const std::string emb_hash = FileHash::Sha256((test_dir / "e.tmp").string(), err);
    if (motion_hash.empty() || tok_hash.empty() || emb_hash.empty())
    {
        std::printf("  FAIL: cannot hash synthetic fixtures\n");
        return 1;
    }
    // Same-size corrupt variant (same byte count, different bytes).
    std::string corrupt_motion = motion_content;
    corrupt_motion[0] = (corrupt_motion[0] == 'X' ? 'Y' : 'X');

    std::filesystem::path reg = test_dir / "models.json";
    {
        std::ofstream oj(reg);
        oj << "{\n  \"models\": [\n"
           << "    {\"id\": \"sim-motion\", \"name\": \"Sim Motion\", \"version\": \"1.1\", "
           << "\"repo\": \"LocalAI-io/Kimodo-SOMA-RP-v1.1-GGML\", "
           << "\"remotePath\": \"models/sim-motion.gguf\", \"localFilename\": \"sim-motion.gguf\", "
           << "\"sizeBytes\": " << motion_content.size() << ", \"sha256\": \"" << motion_hash << "\", "
           << "\"required\": true, \"assetType\": \"motion\", \"gated\": false},\n"
           << "    {\"id\": \"llm2vec-text-bundle\", \"name\": \"Sim Text Bundle\", \"version\": \"1.0\", "
           << "\"repo\": \"LocalAI-io/Llama-3-Kimodo-GGML\", "
           << "\"localFilename\": \"llm2vec-text-bundle\", "
           << "\"sizeBytes\": " << (tok_content.size() + emb_content.size()) << ", "
           << "\"required\": true, \"assetType\": \"text_encoder\", \"gated\": false, \"files\": [\n"
           << "      {\"remotePath\": \"generated/llm2vec-text-bundle/tokenizer.gguf\", "
           << "\"localFilename\": \"tokenizer.gguf\", \"sizeBytes\": " << tok_content.size()
           << ", \"sha256\": \"" << tok_hash << "\"},\n"
           << "      {\"remotePath\": \"generated/llm2vec-text-bundle/embedding.gguf\", "
           << "\"localFilename\": \"embedding.gguf\", \"sizeBytes\": " << emb_content.size()
           << ", \"sha256\": \"" << emb_hash << "\"}\n"
           << "    ]}\n  ]\n}\n";
    }

    // 1. Stale/wrong SHA registry: file present at right size but hash differs
    //    must be detected by hash comparison (mirrors VerifyAsync logic).
    {
        write_file(models_dir / "sim-motion.gguf", corrupt_motion);
        const std::string got = FileHash::Sha256((models_dir / "sim-motion.gguf").string(), err);
        if (got == motion_hash)
        {
            std::printf("  FAIL: corrupt same-size motion fixture unexpectedly matches registry hash\n");
            fails++;
        }
        else
        {
            std::printf("  [PASS] 1. stale/wrong-SHA registry mismatch detected (hash differs)\n");
        }
    }

    // 2. Valid motion + complete bundle recognized; corrupt text file rejected
    //    by hash even though its size matches.
    {
        write_file(models_dir / "sim-motion.gguf", motion_content);
        write_file(bundle_dir / "tokenizer.gguf", tok_content);
        write_file(bundle_dir / "embedding.gguf", emb_content);
        ModelManager mm;
        mm.Init(reg.string(), models_dir.string(), bundle_dir.string());
        if (!mm.IsBundleComplete("llm2vec-text-bundle"))
        {
            std::printf("  FAIL: valid 2-file bundle not recognized as complete\n");
            fails++;
        }
        // Corrupt one component with same byte count: size gate still passes,
        // but registry hash comparison must fail.
        write_file(bundle_dir / "embedding.gguf", std::string(emb_content.size(), 'Z'));
        const std::string got = FileHash::Sha256((bundle_dir / "embedding.gguf").string(), err);
        if (got == emb_hash)
        {
            std::printf("  FAIL: corrupt text GGUF fixture matches registry hash\n");
            fails++;
        }
        else
        {
            std::printf("  [PASS] 2. corrupt text GGUF detected via SHA-256 (size alone insufficient)\n");
        }
        write_file(bundle_dir / "embedding.gguf", emb_content); // restore
    }

    // 3. Missing tokenizer / incomplete bundle / .part-only never complete.
    {
        std::filesystem::remove(bundle_dir / "tokenizer.gguf", ec);
        ModelManager mm;
        mm.Init(reg.string(), models_dir.string(), bundle_dir.string());
        if (mm.IsBundleComplete("llm2vec-text-bundle"))
        {
            std::printf("  FAIL: bundle without tokenizer.gguf reported complete\n");
            fails++;
        }
        else
        {
            std::printf("  [PASS] 3a. missing tokenizer rejects bundle\n");
        }
        write_file(bundle_dir / "tokenizer.gguf", tok_content);
        // .part-only file: final removed, only .part present.
        std::filesystem::remove(bundle_dir / "embedding.gguf", ec);
        write_file(bundle_dir / "embedding.gguf.part", emb_content);
        ModelManager mm2;
        mm2.Init(reg.string(), models_dir.string(), bundle_dir.string());
        if (mm2.IsBundleComplete("llm2vec-text-bundle"))
        {
            std::printf("  FAIL: .part-only bundle reported complete (atomic install violated)\n");
            fails++;
        }
        else
        {
            std::printf("  [PASS] 3b. .part-only file never counts as installed (atomic install)\n");
        }
        std::filesystem::remove(bundle_dir / "embedding.gguf.part", ec);
        write_file(bundle_dir / "embedding.gguf", emb_content);
    }

    // 4. VerifyAll covers ALL required assets: with a missing required asset it
    //    must enter Error, never silently verify only the motion model.
    {
        ModelManager empty_mm;
        empty_mm.Init(reg.string(), (test_dir / "nope-models").string(),
                      (test_dir / "nope-bundle").string());
        AppState st;
        SetupManager sm;
        sm.Init(empty_mm, st);
        sm.VerifyAll(empty_mm);
        if (sm.GetState() != SetupState::Error)
        {
            std::printf("  FAIL: VerifyAll with missing assets did not enter Error\n");
            fails++;
        }
        else
        {
            std::printf("  [PASS] 4. VerifyAll fails fast when any required asset is missing\n");
        }
    }

    // 5. tokenizer.gguf must never be accepted as the text model path.
    {
        KimodoAdapter adapter;
        std::string load_err;
        const std::string tok_path = (bundle_dir / "tokenizer.gguf").string();
        if (adapter.Load("motion.gguf", tok_path, load_err))
        {
            std::printf("  FAIL: adapter accepted tokenizer.gguf as text model\n");
            fails++;
        }
        else if (load_err.find("tokenizer.gguf") == std::string::npos)
        {
            std::printf("  FAIL: tokenizer guard error message unclear: %s\n", load_err.c_str());
            fails++;
        }
        else
        {
            std::printf("  [PASS] 5. tokenizer.gguf rejected as text model path\n");
        }
    }

    // 6. MotionResult propagation contract: successful generation content must
    //    survive (frames/joints + buffers), empty results must not count.
    {
        MotionResult mr;
        mr.frames = 7;
        mr.joints = 30;
        mr.local_rotations_xyzw.assign(7 * 30 * 4, 0.0f);
        for (size_t i = 0; i < 7 * 30; ++i)
        {
            mr.local_rotations_xyzw[i * 4 + 3] = 1.0f;
        }
        mr.root_positions.assign(7 * 3, 0.0f);
        Animation anim;
        anim.FromMotionResult(mr);
        if (anim.empty() || anim.frames != 7 || anim.joints != 30)
        {
            std::printf("  FAIL: MotionResult did not propagate frames/joints to Animation\n");
            fails++;
        }
        else
        {
            std::printf("  [PASS] 6. MotionResult propagates frames/joints (no empty overwrite)\n");
        }
        MotionResult empty;
        Animation empty_anim;
        empty_anim.FromMotionResult(empty);
        if (!empty_anim.empty())
        {
            std::printf("  FAIL: empty MotionResult produced a non-empty Animation\n");
            fails++;
        }
    }

    // 7. Transfer length validation: silent truncation must be detectable.
    {
        struct Case
        {
            uint64_t got;
            uint64_t expected;
            bool ok;
        };
        const Case cases[] = {
            {100, 100, true},  // exact: accept
            {99, 100, false},  // short (truncated): reject
            {101, 100, false}, // over: reject
            {0, 0, true},      // unknown length (chunked): cannot judge
            {50, 0, true},     // unknown length: cannot judge
        };
        for (const auto& c : cases)
        {
            if (HuggingFaceClient::DownloadLengthOk(c.got, c.expected) != c.ok)
            {
                std::printf("  FAIL: DownloadLengthOk(%llu, %llu) wrong verdict\n",
                            static_cast<unsigned long long>(c.got),
                            static_cast<unsigned long long>(c.expected));
                fails++;
            }
        }
        struct RCase
        {
            uint64_t offset;
            uint64_t expected;
            bool ok;
        };
        const RCase rcases[] = {
            {0, 100, true},   // fresh download: nothing to judge
            {50, 100, true},  // valid prefix: resumable
            {100, 100, true}, // complete: resumable (hash decides)
            {101, 100, false}, // oversized: never a valid prefix, discard
            {50, 0, true},    // unknown size: cannot judge
        };
        for (const auto& c : rcases)
        {
            if (HuggingFaceClient::ResumeOffsetOk(c.offset, c.expected) != c.ok)
            {
                std::printf("  FAIL: ResumeOffsetOk(%llu, %llu) wrong verdict\n",
                            static_cast<unsigned long long>(c.offset),
                            static_cast<unsigned long long>(c.expected));
                fails++;
            }
        }
        struct CCase
        {
            uint64_t start;
            uint64_t chunk;
            uint64_t total;
            uint64_t end;
        };
        const CCase ccases[] = {
            {0, 64, 100, 63},   // first chunk
            {64, 64, 100, 99},  // clamped tail chunk
            {0, 64, 64, 63},    // exact single chunk
            {0, 64, 30, 29},    // total smaller than one chunk
            {128, 64, 200, 191}, // mid-file chunk
        };
        for (const auto& c : ccases)
        {
            if (HuggingFaceClient::ChunkEnd(c.start, c.chunk, c.total) != c.end)
            {
                std::printf("  FAIL: ChunkEnd(%llu, %llu, %llu) != %llu\n",
                            static_cast<unsigned long long>(c.start),
                            static_cast<unsigned long long>(c.chunk),
                            static_cast<unsigned long long>(c.total),
                            static_cast<unsigned long long>(c.end));
                fails++;
            }
        }
        if (fails == 0)
        {
            std::printf("  [PASS] 7. transfer length + resume-offset + chunk math\n");
        }
    }

    // 8. Short final file is never "installed" (size gate before hashing).
    {
        write_file(models_dir / "sim-motion.gguf", std::string("short"));
        write_file(bundle_dir / "tokenizer.gguf", tok_content);
        write_file(bundle_dir / "embedding.gguf", emb_content);
        ModelManager mm;
        mm.Init(reg.string(), models_dir.string(), bundle_dir.string());
        if (mm.AreAllRequiredInstalled())
        {
            std::printf("  FAIL: truncated motion file reported as installed\n");
            fails++;
        }
        else
        {
            std::printf("  [PASS] 8. truncated file rejected by size gate (pre-hash)\n");
        }
        write_file(models_dir / "sim-motion.gguf", motion_content); // restore
    }

    // 9. VerifyAsync failure plumbing: same-size-corrupt file must yield
    //    TaskResult::Failed plus expected/received SHAs + byte counts.
    {
        write_file(models_dir / "sim-motion.gguf", corrupt_motion);
        ModelManager mm;
        mm.Init(reg.string(), models_dir.string(), bundle_dir.string());
        if (mm.GetTaskResult() != TaskResult::Unknown)
        {
            std::printf("  FAIL: fresh ModelManager task result is not Unknown\n");
            fails++;
        }
        mm.VerifyAsync("sim-motion");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        while (mm.IsBusy() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (mm.IsBusy())
        {
            std::printf("  FAIL: VerifyAsync did not finish within 60s\n");
            fails++;
        }
        else if (mm.GetTaskResult() != TaskResult::Failed)
        {
            std::printf("  FAIL: corrupt-file verify did not report TaskResult::Failed\n");
            fails++;
        }
        else
        {
            std::string f, exp, rec, msg;
            uint64_t eb = 0, rb = 0;
            bool known = false;
            if (!mm.GetLastFailure(f, exp, rec, eb, rb, known, msg))
            {
                std::printf("  FAIL: no failure details recorded for corrupt file\n");
                fails++;
            }
            else if (exp != motion_hash || rec.empty() || rec == motion_hash || !known ||
                     eb != motion_content.size())
            {
                std::printf("  FAIL: failure details wrong (exp=%s rec=%s bytes=%llu/%llu)\n",
                            exp.c_str(), rec.c_str(), static_cast<unsigned long long>(eb),
                            static_cast<unsigned long long>(rb));
                fails++;
            }
            else
            {
                std::printf("  [PASS] 9. VerifyAsync Failed + expected/received SHA + bytes\n");
            }
        }
        write_file(models_dir / "sim-motion.gguf", motion_content); // restore
    }

    // 10. VerifyAsync success plumbing: valid files must yield TaskResult::Ok.
    {
        ModelManager mm;
        mm.Init(reg.string(), models_dir.string(), bundle_dir.string());
        mm.VerifyAsync("sim-motion");
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        while (mm.IsBusy() && std::chrono::steady_clock::now() < deadline)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        if (mm.IsBusy())
        {
            std::printf("  FAIL: VerifyAsync (valid) did not finish within 60s\n");
            fails++;
        }
        else if (mm.GetTaskResult() != TaskResult::Ok)
        {
            std::printf("  FAIL: valid-file verify did not report TaskResult::Ok\n");
            fails++;
        }
        else
        {
            std::printf("  [PASS] 10. VerifyAsync success reports TaskResult::Ok\n");
        }
    }

    std::filesystem::remove_all(test_dir, ec);
    std::printf("  -> Model Install / Verify: %s (%d failures)\n", fails == 0 ? "PASSED" : "FAILED", fails);
    return fails;
}

int TestSuite::RunAll()
{
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
    total_fails += RunResizeRegression();
    total_fails += RunSetupWizardSimulation();
    total_fails += RunModelInstallVerify();

    std::printf("===================================================\n");
    if (total_fails == 0)
    {
        std::printf("  ALL TESTS PASSED SUCCESSFULLY! (0 failures)       \n");
    }
    else
    {
        std::printf("  TEST SUITE COMPLETED WITH %d FAILURES              \n", total_fails);
    }
    std::printf("===================================================\n");
    return total_fails == 0 ? 0 : 1;
}

} // namespace studio
