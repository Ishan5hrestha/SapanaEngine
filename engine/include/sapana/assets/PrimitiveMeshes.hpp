#pragma once

#include "RenderDevice.h"
#include "sapana/assets/MeshAsset.hpp"

namespace sapana
{
namespace assets
{

/// Built-in procedural meshes for Basic rendering.
namespace PrimitiveMeshes
{

MeshAssetPtr CreateCube(Diligent::IRenderDevice* device);

/// Unit XZ plane in [-1, 1], y = 0, facing +Y. Scale via Transform for world size.
MeshAssetPtr CreatePlane(Diligent::IRenderDevice* device);

} // namespace PrimitiveMeshes
} // namespace assets
} // namespace sapana
