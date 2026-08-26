#pragma once

#include "DeviceContext.h"
#include "RenderDevice.h"
#include "sapana/assets/MeshAsset.hpp"

#include <string>

namespace sapana
{
namespace assets
{

/// Loads a glTF / GLB file into a Sapana MeshAsset (Basic path) and optionally a Diligent GLTF::Model (PBR).
/// Artists may author in FBX; convert offline to glTF 2.0 before dropping files under assets/meshes/.
class GltfMeshLoader
{
public:
    /// Loads geometry for the Basic forward PSO (position + color) and a full GLTF::Model for PBR.
    /// Returns nullptr on failure.
    static MeshAssetPtr Load(Diligent::IRenderDevice*  device,
                             Diligent::IDeviceContext* context,
                             const std::string&        path);
};

} // namespace assets
} // namespace sapana
