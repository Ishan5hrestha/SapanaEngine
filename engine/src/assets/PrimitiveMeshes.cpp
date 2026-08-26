#include "sapana/assets/PrimitiveMeshes.hpp"

#include "BasicMath.hpp"
#include "Buffer.h"

namespace sapana
{
namespace assets
{
namespace PrimitiveMeshes
{

namespace
{

using Diligent::BIND_INDEX_BUFFER;
using Diligent::BIND_VERTEX_BUFFER;
using Diligent::BufferData;
using Diligent::BufferDesc;
using Diligent::float3;
using Diligent::float4;
using Diligent::USAGE_IMMUTABLE;
using Diligent::VT_UINT32;

struct Vertex
{
    float3 pos;
    float4 color;
};

} // namespace

MeshAssetPtr CreateCube(Diligent::IRenderDevice* device)
{
    if (device == nullptr)
        return nullptr;

    constexpr Vertex CubeVerts[8] = {
        {float3{-1, -1, -1}, float4{1, 0, 0, 1}},
        {float3{-1, +1, -1}, float4{0, 1, 0, 1}},
        {float3{+1, +1, -1}, float4{0, 0, 1, 1}},
        {float3{+1, -1, -1}, float4{1, 1, 1, 1}},
        {float3{-1, -1, +1}, float4{1, 1, 0, 1}},
        {float3{-1, +1, +1}, float4{0, 1, 1, 1}},
        {float3{+1, +1, +1}, float4{1, 0, 1, 1}},
        {float3{+1, -1, +1}, float4{0.2f, 0.2f, 0.2f, 1.f}},
    };

    constexpr Diligent::Uint32 Indices[] = {
        2, 0, 1, 2, 3, 0,
        4, 6, 5, 4, 7, 6,
        0, 7, 4, 0, 3, 7,
        1, 0, 4, 1, 4, 5,
        1, 5, 2, 5, 6, 2,
        3, 6, 7, 3, 2, 6};

    auto mesh = std::make_shared<MeshAsset>();

    BufferDesc VertBuffDesc;
    VertBuffDesc.Name      = "Builtin cube VB";
    VertBuffDesc.Usage     = USAGE_IMMUTABLE;
    VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc.Size      = sizeof(CubeVerts);
    BufferData VBData;
    VBData.pData    = CubeVerts;
    VBData.DataSize = sizeof(CubeVerts);
    device->CreateBuffer(VertBuffDesc, &VBData, &mesh->VertexBuffer);

    BufferDesc IndBuffDesc;
    IndBuffDesc.Name      = "Builtin cube IB";
    IndBuffDesc.Usage     = USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
    IndBuffDesc.Size      = sizeof(Indices);
    BufferData IBData;
    IBData.pData    = Indices;
    IBData.DataSize = sizeof(Indices);
    device->CreateBuffer(IndBuffDesc, &IBData, &mesh->IndexBuffer);

    mesh->IndexCount = static_cast<Diligent::Uint32>(sizeof(Indices) / sizeof(Indices[0]));
    mesh->IndexType  = VT_UINT32;
    return mesh;
}

MeshAssetPtr CreatePlane(Diligent::IRenderDevice* device)
{
    if (device == nullptr)
        return nullptr;

    // Unit quad on XZ, normal +Y. White verts; tint comes from MeshRenderer.Color.
    constexpr Vertex PlaneVerts[4] = {
        {float3{-1.f, 0.f, -1.f}, float4{1.f, 1.f, 1.f, 1.f}},
        {float3{+1.f, 0.f, -1.f}, float4{1.f, 1.f, 1.f, 1.f}},
        {float3{+1.f, 0.f, +1.f}, float4{1.f, 1.f, 1.f, 1.f}},
        {float3{-1.f, 0.f, +1.f}, float4{1.f, 1.f, 1.f, 1.f}},
    };

    // CCW when viewed from +Y (matches back-face culling).
    constexpr Diligent::Uint32 Indices[] = {
        0, 2, 1,
        0, 3, 2,
    };

    auto mesh = std::make_shared<MeshAsset>();

    BufferDesc VertBuffDesc;
    VertBuffDesc.Name      = "Builtin plane VB";
    VertBuffDesc.Usage     = USAGE_IMMUTABLE;
    VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc.Size      = sizeof(PlaneVerts);
    BufferData VBData;
    VBData.pData    = PlaneVerts;
    VBData.DataSize = sizeof(PlaneVerts);
    device->CreateBuffer(VertBuffDesc, &VBData, &mesh->VertexBuffer);

    BufferDesc IndBuffDesc;
    IndBuffDesc.Name      = "Builtin plane IB";
    IndBuffDesc.Usage     = USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
    IndBuffDesc.Size      = sizeof(Indices);
    BufferData IBData;
    IBData.pData    = Indices;
    IBData.DataSize = sizeof(Indices);
    device->CreateBuffer(IndBuffDesc, &IBData, &mesh->IndexBuffer);

    mesh->IndexCount = static_cast<Diligent::Uint32>(sizeof(Indices) / sizeof(Indices[0]));
    mesh->IndexType  = VT_UINT32;
    return mesh;
}

} // namespace PrimitiveMeshes
} // namespace assets
} // namespace sapana
