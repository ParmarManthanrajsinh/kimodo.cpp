# KIMODO STUDIO — FINAL RETARGETING / EXPORT SIMPLIFICATION

We are simplifying the animation pipeline.

The previous implementation attempted to build a complete Unreal Engine Manny retargeting system inside Kimodo Studio. This is unnecessarily complex because Unreal already provides a professional IK Rig / IK Retargeter ecosystem.

DO NOT continue building a custom Unreal Manny retargeter.

The final architecture should be:

                    Kimodo Studio
                         |
                 Generate Animation
                         |
                  Generic Motion
                    /          \
                   /            \
            Blender             BVH
          Retargeting            |
                                |
                                v
                         Unreal Engine
                         IK Retargeter
                                |
                                v
                         UE5 Manny / MetaHuman
                         / other UE skeletons


============================================================
GOAL
============================================================

Simplify Kimodo Studio so that it focuses on:

1. Kimodo motion generation
2. Animation preview
3. Animation library
4. Generic Blender retargeting
5. High-quality BVH export
6. Import/export workflow suitable for Unreal Engine

REMOVE the need for Kimodo to implement Unreal's own retargeting system.

DO NOT implement:
- custom UE5 Manny retargeting
- custom Unreal IK Retargeter
- Unreal-specific IK solving
- custom UE chain solving
- Unreal skeletal mesh import/export
- UE-specific FBX generation
- custom UE pelvis/foot IK
- Unreal-specific reference-pose solving

Unreal Engine will handle those tasks.

============================================================
PHASE 1 — REMOVE / DEPRECATE UE RETARGETING
============================================================

Inspect:

- kimodo-studio/src/retarget/Retargeter.cpp
- kimodo-studio/src/retarget/Retargeter.h
- kimodo-studio/src/retarget/SkeletonProfile.cpp
- kimodo-studio/src/retarget/SkeletonProfile.h
- kimodo-studio/src/app/UIManager.cpp
- export-related files
- library-related files
- tests in main.cpp

Remove the requirement for the custom:

unreal-manny

retargeting pipeline.

Do not simply delete code blindly.

Preserve useful generic retargeting infrastructure if it is shared with Blender.

The Unreal-specific chain system can either be removed or isolated/deprecated.

The application should no longer advertise that it performs professional UE5 Manny retargeting internally.

============================================================
PHASE 2 — BLENDER SUPPORT
============================================================

KEEP Blender support.

The existing:

blender-generic

profile should remain supported.

IMPORTANT:

A previous retargeter change broke Blender retargeting.

Restore Blender generic retargeting to a stable and predictable implementation.

Blender generic should use the source skeleton's humanoid structure and direct mapping where appropriate.

Do not apply UE5-specific:
- bind-pose normalization
- Manny rest-pose reconstruction
- UE chain resampling
- UE-specific root corrections
- UE-specific IK
- UE-specific pelvis behavior

Blender retargeting should remain independent of the deprecated UE5 retargeter.

Verify:

- Hips
- Spine
- Chest
- Neck
- Head
- arms
- legs
- feet

with normal animation.

Add regression tests for Blender.

Tests must include:

1. identity/reference pose
2. root rotation
3. spine rotation
4. arm rotation
5. leg rotation
6. walking animation
7. quaternion normalization
8. no NaN/Inf
9. no mirrored limbs
10. no collapsed skeleton

Do not proceed until Blender retargeting works correctly.

============================================================
PHASE 3 — BVH EXPORT BECOMES THE MAIN UE PIPELINE
============================================================

BVH export should become a first-class production feature.

The intended Unreal workflow is:

Kimodo Studio
    -> Export BVH
    -> Import BVH into Unreal
    -> UE IK Rig
    -> UE IK Retargeter
    -> Manny / desired target skeleton

The BVH must therefore contain clean, predictable humanoid motion.

Do not try to encode Unreal-specific behavior into the BVH.

============================================================
PHASE 4 — BVH EXPORT QUALITY
============================================================

Inspect the existing BVH exporter.

Verify:

- hierarchy
- ROOT declaration
- JOINT hierarchy
- End Sites
- CHANNELS
- OFFSET
- frame count
- frame time
- rotations
- root translation
- root rotation
- coordinate system
- units
- quaternion -> Euler conversion
- Euler order
- numerical stability

The exporter must produce standards-compliant BVH.

Use the existing animation skeleton topology.

Do not hard-code Manny into the BVH exporter.

============================================================
PHASE 5 — BVH COORDINATE SYSTEM
============================================================

Define one explicit BVH coordinate convention.

Document:

- up axis
- forward axis
- right axis
- handedness
- unit convention
- Euler rotation order

Do not introduce arbitrary axis swaps.

The conversion should happen in one clearly defined place.

For example:

Internal Kimodo coordinates
        |
        v
BVH coordinate conversion
        |
        v
BVH

Do not scatter coordinate conversions throughout the exporter.

Make the conversion deterministic.

============================================================
PHASE 6 — ROOT MOTION
============================================================

Handle root motion correctly.

BVH root must contain:

- root translation
- root rotation

Child bones should contain their rotations.

Do not duplicate root rotation into pelvis/hips.

Do not duplicate root translation into child joints.

Verify that a root-only rotation test produces:

ROOT rotation changed
all child local rotations unchanged

and that a root translation test produces:

ROOT position changed
child offsets unchanged.

============================================================
PHASE 7 — EULER CONVERSION
============================================================

BVH requires Euler rotations.

The internal animation uses quaternions.

Implement robust:

Quaternion -> rotation matrix -> Euler

conversion.

Requirements:

- deterministic rotation order
- normalized quaternion input
- stable behavior near singularities
- no NaN/Inf
- continuous animation where possible
- minimize frame-to-frame Euler discontinuities

Add tests using:

- identity
- 90° X
- 90° Y
- 90° Z
- combined rotations
- near-gimbal configurations

Round-trip test:

Quaternion
 -> BVH Euler
 -> Quaternion

must remain within tolerance.

============================================================
PHASE 8 — BVH HUMANOID EXPORT PROFILE
============================================================

Create a dedicated export preset:

id:
bvh-humanoid

name:
BVH Humanoid

This should be generic.

It must not be called:

Unreal Manny BVH

because the purpose is not to create an Unreal-specific skeleton.

The export preset should expose:

- FPS
- scale
- root motion on/off
- coordinate basis
- rotation order
- optional frame range

Keep defaults sensible.

============================================================
PHASE 9 — EXPORT UI
============================================================

Simplify the export UI.

The user should see something like:

Export Animation

Format:
[ BVH ]

Preset:
[ Humanoid ]

FPS:
[ 30 ]

Scale:
[ ... ]

Root Motion:
[ Enabled ]

Rotation Order:
[ XYZ ]

[ Export BVH ]

Do not expose unnecessary Unreal-specific options.

If the application currently has:

Unreal Engine

as a custom export preset, replace its behavior with:

BVH Humanoid

or clearly mark the old Unreal preset as deprecated.

============================================================
PHASE 10 — UNREAL WORKFLOW DOCUMENTATION
============================================================

Add documentation explaining the intended Unreal workflow.

Example:

1. Generate animation in Kimodo Studio.
2. Export as BVH.
3. Import the BVH into Unreal Engine.
4. Create/use an IK Rig for the imported source skeleton.
5. Create/use an IK Rig for UE5 Manny.
6. Create an IK Retargeter.
7. Set the BVH skeleton as Source.
8. Set Manny as Target.
9. Configure source/target chains in Unreal.
10. Adjust Retarget Pose if necessary.
11. Use Unreal's Pelvis Motion / IK settings.
12. Export/bake the retargeted animation to the desired Unreal asset.

Make it clear that:

Kimodo generates the motion.

Unreal performs the final professional retargeting.

============================================================
PHASE 11 — BLENDER WORKFLOW
============================================================

Document the Blender workflow separately.

Kimodo:

Generate
    ->
Blender Generic Retarget
    ->
Preview
    ->
Export BVH

or:

Generate
    ->
Export BVH
    ->
Blender import

Do not make Blender dependent on the Unreal code path.

============================================================
PHASE 12 — FILE FORMAT VALIDATION
============================================================

Create a BVH validation test.

After exporting a test animation:

1. Read the generated BVH back.
2. Validate hierarchy.
3. Validate joint count.
4. Validate frame count.
5. Validate frame time.
6. Validate channels.
7. Validate numeric values.
8. Validate rotations.
9. Validate root translation.

If a BVH reader already exists, reuse it.

Otherwise implement a lightweight internal validation parser.

Do not add a large third-party dependency just for validation.

============================================================
PHASE 13 — ROUND TRIP TEST
============================================================

Create:

Animation
    ->
BVH export
    ->
BVH import
    ->
Animation

Then compare:

- frame count
- joint count
- hierarchy
- root translation
- joint rotations

Use tolerances appropriate for Euler conversion.

The round-trip should not introduce visible motion corruption.

============================================================
PHASE 14 — REGRESSION TESTS
============================================================

Required tests:

BVH:

1. identity animation
2. root translation
3. root yaw
4. root pitch
5. spine rotation
6. arm rotation
7. leg rotation
8. combined walking animation
9. multiple frames
10. 30 FPS
11. 60 FPS
12. non-zero root position
13. quaternion normalization
14. Euler round trip
15. no NaN/Inf
16. hierarchy validation

Blender:

1. identity
2. root rotation
3. spine
4. arms
5. legs
6. walking animation
7. no mirrored limbs
8. no collapsed torso

============================================================
PHASE 15 — CLEAN UP CURRENT UE CODE
============================================================

Search the entire repository for:

unreal-manny
UE5 Manny
Manny
Retargeter
IK
Unreal Engine

Determine which code is now unnecessary.

Do not delete useful generic retarget functionality.

Remove dead code only after verifying that it is no longer referenced.

Remove misleading UI and comments claiming that Kimodo performs UE5 professional retargeting.

The project should communicate the new architecture clearly:

"Export motion from Kimodo and use Unreal Engine's IK Retargeter for final UE skeleton retargeting."

============================================================
PHASE 16 — DO NOT BREAK EXISTING FEATURES
============================================================

Preserve:

- Kimodo inference
- model manager
- model installation
- Hugging Face integration
- animation generation
- animation viewer
- animation library
- library persistence
- Blender generic retargeting
- BVH export
- GLB export if currently stable
- application packaging
- settings
- existing UI architecture

Do not remove unrelated features.

============================================================
PHASE 17 — FINAL UI
============================================================

Retarget page should focus on:

Target:
[ Blender Generic ]

Source:
[ Current Animation ]

Mapping:
[ Auto Map ]

Preview
Apply

If Unreal is selected, do NOT present a fake internal UE5 Manny retargeter.

Instead provide:

"Unreal Engine Workflow"

Generate a BVH and use Unreal Engine's IK Retargeter for final retargeting.

Button:

[ Export BVH ]

Optionally:

[ Open Export Folder ]

============================================================
PHASE 18 — FINAL ACCEPTANCE CRITERIA
============================================================

The implementation is complete only when:

1. Blender retargeting works again.
2. BVH exports correctly.
3. BVH hierarchy is valid.
4. BVH rotations are correct.
5. Root motion is correct.
6. BVH round-trip passes.
7. No NaN/Inf values occur.
8. No mirrored animation occurs.
9. No unexpected root/pelvis duplication occurs.
10. Existing animation generation still works.
11. Existing library functionality still works.
12. Existing packaging still works.
13. No custom UE5 Manny retargeting is required.
14. The Unreal workflow is documented.
15. The application UI no longer implies that Kimodo replaces Unreal's IK Retargeter.

============================================================
FINAL ARCHITECTURE
============================================================

The final system should be:

                KIMODO STUDIO
                     |
                     v
              Motion Generation
                     |
                     v
             Animation / Library
                     |
          +----------+----------+
          |                     |
          v                     v
   Blender Generic             BVH
      Retarget                 Export
          |                     |
          v                     v
       Preview             Unreal Engine
                                |
                                v
                         IK Rig / IK Retargeter
                                |
                                v
                         UE5 Manny / Target

IMPORTANT:

Do not over-engineer the Unreal side.

The purpose of this change is to make Kimodo Studio smaller, more reliable, easier to maintain, and compatible with professional Unreal workflows by leveraging Unreal's existing retargeting ecosystem.

At the end, provide:

- files changed
- files removed/deprecated
- Blender regression cause
- Blender fix
- BVH exporter changes
- coordinate convention
- Euler rotation order
- root-motion behavior
- BVH round-trip test results
- Blender test results
- build result
- packaging result
- final Unreal workflow
- any remaining limitations
