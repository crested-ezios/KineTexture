#include <KineTexture/KineTexture.hpp>
#include <cmath>
#include <cstdio>

// Explicit Hamilton unit-quaternion vector rotation (v + w*t + q.xyz cross t,
// t = 2 * q.xyz cross v). This example uses only public value types, not helpers
// in NativeDetail, so it can also show the FK equation to application authors.
static KineTexture::Vector3d RotateUnit(KineTexture::Quaterniond q, KineTexture::Vector3d v)
{
    const KineTexture::Vector3d t{2 * (q.y * v.z - q.z * v.y),
        2 * (q.z * v.x - q.x * v.z), 2 * (q.x * v.y - q.y * v.x)};
    return {v.x + q.w * t.x + q.y * t.z - q.z * t.y,
        v.y + q.w * t.y + q.z * t.x - q.x * t.z,
        v.z + q.w * t.z + q.x * t.y - q.y * t.x};
}

int main()
{
    // Positions and offsets use meters in a caller-declared right-handed frame.
    // Segment offsets are local +X; identity world hints preserve their roll.
    KineTexture::TwoBoneInput input;
    input.upper_offset = {1, 0, 0};
    input.lower_offset = {1, 0, 0};
    input.goal_position = {1, 1, 0};
    input.pole_hint = KineTexture::Vector3d{0, 1, 0};
    KineTexture::TwoBoneSolution solution;
    if (!KineTexture::SolveTwoBone(input, solution)) return 1;

    // The returned world rotations reproduce the joint/end through FK. Do not
    // assign them as local rotations without converting through the parent.
    const auto upper = RotateUnit(solution.upper_world, input.upper_offset);
    const auto lower = RotateUnit(solution.lower_world, input.lower_offset);
    const KineTexture::Vector3d end{
        input.root_position.x + upper.x + lower.x,
        input.root_position.y + upper.y + lower.y,
        input.root_position.z + upper.z + lower.z};
    std::printf("joint=(%.3f, %.3f, %.3f), end=(%.3f, %.3f, %.3f), residual=%.3g\n",
        solution.joint_position.x, solution.joint_position.y, solution.joint_position.z,
        end.x, end.y, end.z, solution.residual);
    if (!solution.reachable || solution.residual > 1e-10 ||
        std::abs(end.x - 1) > 1e-10 || std::abs(end.y - 1) > 1e-10 ||
        std::abs(end.z) > 1e-10) return 2;

    input.goal_position = {4, 0, 0};
    if (!KineTexture::SolveTwoBone(input, solution)) return 3;
    std::printf("unreachable goal: end.x=%.3f, residual=%.3f\n",
        solution.end_position.x, solution.residual);
    return !solution.reachable && std::abs(solution.end_position.x - 2) < 1e-10 &&
        std::abs(solution.residual - 2) < 1e-10 ? 0 : 4;
}
