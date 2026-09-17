# Unreal Workflow — BVH Humanoid + IK Retargeter

Kimodo generates motion. Unreal retargets. Kimodo does NOT replace
Unreal IK Rig / IK Retargeter.

## BVH convention

Y-up, right-handed, meters, XYZ Euler degrees. ROOT = translation +
rotation. Children = rotation only. Single conversion in
`prepareExport`. No Manny encoding in BVH.

## Steps

1. Generate animation in Kimodo Studio.
2. Library -> Export -> Format BVH, Preset BVH Humanoid, FPS 30,
   Scale 1, Root Motion Preserve, Rotation XYZ.
3. Import BVH into Unreal (skeleton = source).
4. Create/use IK Rig for imported source skeleton.
5. Create/use IK Rig for UE5 Manny / MetaHuman target.
6. Create IK Retargeter. Source = BVH skeleton. Target = Manny.
7. Configure source/target chains in Unreal.
8. Adjust Retarget Pose if needed.
9. Use Unreal Pelvis Motion / IK settings.
10. Bake/export retargeted animation to Unreal asset.

## Blender path (separate)

Generate -> Blender Generic Retarget -> Preview -> Export BVH,
or Generate -> Export BVH -> Blender import.
Blender Generic = direct local copy, no UE bind/IK/chain logic.
