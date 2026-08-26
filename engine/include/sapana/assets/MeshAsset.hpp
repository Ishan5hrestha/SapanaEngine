#pragma once

#include "RenderDevice.h"
#include "RefCntAutoPtr.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace Diligent
{
namespace GLTF
{
struct Model;
} // namespace GLTF
} // namespace Diligent

namespace sapana
{
namespace assets
{

/// GPU mesh for the Basic forward path (position + color vertices).
/// Optionally retains a Diligent GLTF::Model for the PBR path.
struct MeshAsset
{
    Diligent::RefCntAutoPtr<Diligent::IBuffer> VertexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> IndexBuffer;
    Diligent::Uint32                          IndexCount = 0;
    Diligent::VALUE_TYPE                      IndexType  = Diligent::VT_UINT32;

    /// Optional: path to source glTF. Empty for builtins.
    std::string SourceGltfPath;

    /// Full glTF model for PBR rendering (null for builtins / Basic-only loads).
    std::shared_ptr<Diligent::GLTF::Model> GltfModel;
};

using MeshAssetPtr = std::shared_ptr<MeshAsset>;

} // namespace assets
} // namespace sapana
