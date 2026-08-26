#pragma once

#include "BasicMath.hpp"

#include <string>
#include <vector>

namespace sapana
{
namespace scene
{

using Diligent::float3;
using Diligent::float4;

struct SceneTransformDesc
{
    float3 Position{0.f, 0.f, 0.f};
    float3 RotationDegrees{0.f, 0.f, 0.f};
    float3 Scale{1.f, 1.f, 1.f};
};

struct SceneMeshDesc
{
    std::string Asset;
    float4      Color{1.f, 1.f, 1.f, 1.f};
};

struct SceneEntityDesc
{
    std::string         Name;
    SceneTransformDesc  Transform;
    SceneMeshDesc       Mesh;
    bool                HasMesh = false;
};

struct SceneDocument
{
    int                           Version = 1;
    std::vector<SceneEntityDesc>  Entities;
};

} // namespace scene
} // namespace sapana
