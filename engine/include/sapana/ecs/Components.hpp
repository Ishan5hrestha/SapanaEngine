#pragma once

#include "BasicMath.hpp"
#include "sapana/assets/AssetId.hpp"

#include <string>

namespace sapana
{
namespace ecs
{

using Diligent::float3;
using Diligent::float4;
using Diligent::float4x4;
using Diligent::PI_F;

struct Name
{
    std::string Value;
};

struct Transform
{
    float3 Position{0.f, 0.f, 0.f};
    float3 RotationDegrees{0.f, 0.f, 0.f};
    float3 Scale{1.f, 1.f, 1.f};

    float4x4 ToMatrix() const
    {
        const float rx = RotationDegrees.x * (PI_F / 180.f);
        const float ry = RotationDegrees.y * (PI_F / 180.f);
        const float rz = RotationDegrees.z * (PI_F / 180.f);

        const float4x4 scale = float4x4::Scale(Scale.x, Scale.y, Scale.z);
        const float4x4 rot =
            float4x4::RotationX(rx) * float4x4::RotationY(ry) * float4x4::RotationZ(rz);
        const float4x4 trans = float4x4::Translation(Position.x, Position.y, Position.z);
        return scale * rot * trans;
    }
};

struct MeshRenderer
{
    assets::AssetId MeshId;
    float4          Color{1.f, 1.f, 1.f, 1.f};
};

} // namespace ecs
} // namespace sapana
