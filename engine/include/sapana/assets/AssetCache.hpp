#pragma once

#include "DeviceContext.h"
#include "RenderDevice.h"
#include "sapana/assets/AssetId.hpp"
#include "sapana/assets/MeshAsset.hpp"

#include <memory>
#include <unordered_map>

namespace sapana
{
namespace assets
{

/// Loads and caches meshes by AssetId (builtins and glTF paths).
class AssetCache
{
public:
    void SetDevice(Diligent::IRenderDevice* device, Diligent::IDeviceContext* context = nullptr)
    {
        m_Device  = device;
        m_Context = context;
    }

    /// Returns cached mesh or creates/loads it. Nullptr on failure.
    MeshAssetPtr GetOrLoad(const AssetId& id);

    void Clear() { m_Meshes.clear(); }

private:
    MeshAssetPtr CreateBuiltin(const AssetId& id);
    MeshAssetPtr LoadGltf(const AssetId& id);

    Diligent::IRenderDevice*                      m_Device  = nullptr;
    Diligent::IDeviceContext*                     m_Context = nullptr;
    std::unordered_map<std::string, MeshAssetPtr> m_Meshes;
};

} // namespace assets
} // namespace sapana
