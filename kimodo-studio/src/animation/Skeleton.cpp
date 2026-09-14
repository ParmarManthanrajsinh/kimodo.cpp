#include "animation/Skeleton.h"

#include "raymath.h"

namespace studio {

std::array<std::string_view, kSomaJoints> Soma30Spec::names{
    "Hips", "Spine1", "Spine2", "Chest", "Neck1", "Neck2", "Head", "Jaw",
    "LeftEye", "RightEye", "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand",
    "LeftHandThumbEnd", "LeftHandMiddleEnd", "RightShoulder", "RightArm", "RightForeArm",
    "RightHand", "RightHandThumbEnd", "RightHandMiddleEnd", "LeftLeg", "LeftShin", "LeftFoot",
    "LeftToeBase", "RightLeg", "RightShin", "RightFoot", "RightToeBase"};

std::array<int, kSomaJoints> Soma30Spec::parents{
    -1, 0, 1, 2, 3, 4, 5, 6, 6, 6, 3, 10, 11, 12, 13, 13, 3, 16, 17, 18, 19, 19,
    0, 22, 23, 24, 0, 26, 27, 28};

std::array<std::array<float, 3>, kSomaJoints> Soma30Spec::offsets{{
    {0, 0, 0},
    {-0.00013727F, 0.0500376256F, -0.00053726669F},
    {-1.86574103e-9F, 0.0712530139F, -0.000298248546F},
    {-5.75188398e-9F, 0.0755006305F, -0.00815970992F},
    {-0.00181676517F, 0.263112953F, -0.00553348292F},
    {-2.85102231e-8F, 0.0770939664F, 0.0230258546F},
    {-4.5975437e-8F, 0.0612891595F, 0.0195370861F},
    {2.63687901e-5F, 0.0047559225F, 0.0309494062F},
    {0.0320638079F, 0.0538020513F, 0.0758688308F},
    {-0.0322244017F, 0.05361869F, 0.0755823359F},
    {0.0162165175F, 0.232371641F, 0.0511341324F},
    {0.149198457F, 2.19397873e-8F, -0.0550232576F},
    {0.287393078F, 2.50268389e-9F, -2.58787737e-5F},
    {0.270939812F, -7.06625108e-9F, 2.60897248e-5F},
    {0.122686267F, -0.0322017573F, 0.0483306876F},
    {0.190119595F, -0.00312878387F, -0.000339570373F},
    {-0.0138011824F, 0.231803086F, 0.0521415786F},
    {-0.150371962F, 1.17387901e-7F, -0.0554560437F},
    {-0.287366393F, 1.87628082e-8F, -2.59709359e-5F},
    {-0.271336198F, -1.16767401e-9F, 2.61269368e-5F},
    {-0.122642483F, -0.0321145448F, 0.0480403904F},
    {-0.190005945F, -0.00306615542F, -0.0003157343F},
    {0.10043214F, -0.0843452671F, 0.0259565473F},
    {-1e-8F, -0.432217537F, -0.00802912805F},
    {1e-8F, -0.421550959F, -0.0348152298F},
    {0, -0.0505947206F, 0.132315294F},
    {-0.10047278F, -0.0829525995F, 0.0262031695F},
    {1e-8F, -0.433622059F, -0.00805555828F},
    {2e-8F, -0.421173943F, -0.0347839785F},
    {-3.42907669e-9F, -0.0507960932F, 0.132841956F},
}};

void Skeleton::forwardKinematics(const float* localXyzw, const float* root,
                                 std::vector<Vector3>& out) {
    std::vector<int> parents(Soma30Spec::parents.begin(), Soma30Spec::parents.end());
    std::vector<std::array<float, 3>> offsets(Soma30Spec::offsets.begin(),
                                              Soma30Spec::offsets.end());
    forwardKinematicsGeneral(localXyzw, root, parents, offsets, out);
}

void Skeleton::forwardKinematicsGeneral(
    const float* localXyzw, const float* root, const std::vector<int>& parents,
    const std::vector<std::array<float, 3>>& offsets, std::vector<Vector3>& out) {
    const int J = static_cast<int>(parents.size());
    out.resize(J);
    std::vector<Quaternion> worldRot(J);
    for (int j = 0; j < J; ++j) {
        Quaternion local{localXyzw[j * 4], localXyzw[j * 4 + 1],
                         localXyzw[j * 4 + 2], localXyzw[j * 4 + 3]};
        local = QuaternionNormalize(local);
        const int p = parents[j];
        if (p < 0 || p >= J) {
            worldRot[j] = local;
            out[j] = {root[0], root[1], root[2]};
        } else {
            worldRot[j] = QuaternionMultiply(worldRot[p], local);
            const auto& o = offsets[j];
            Vector3 off = Vector3RotateByQuaternion({o[0], o[1], o[2]}, worldRot[p]);
            out[j] = Vector3Add(out[p], off);
        }
    }
}

} // namespace studio
