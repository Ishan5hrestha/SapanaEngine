#include "sapana/assets/AssetCache.hpp"

#include "sapana/assets/GltfMeshLoader.hpp"
#include "sapana/assets/PrimitiveMeshes.hpp"

#include <iostream>

namespace sapana
{
namespace assets
{

MeshAssetPtr AssetCache::GetOrLoad(const AssetId& id)
{
    if (id.Empty())
        return nullptr;

    const auto it = m_Meshes.find(id.Str());
    if (it != m_Meshes.end())
        return it->second;

    MeshAssetPtr mesh;
    if (id.Str().rfind("builtin:", 0) == 0)
        mesh = CreateBuiltin(id);
    else
        mesh = LoadGltf(id);

    if (mesh != nullptr)
        m_Meshes.emplace(id.Str(), mesh);
    return mesh;
}

MeshAssetPtr AssetCache::CreateBuiltin(const AssetId& id)
{
    if (m_Device == nullptr)
    {
        std::cerr << "Sapana AssetCache: device is null\n";
        return nullptr;
    }

    if (id.Str() == kBuiltinCubeId)
        return PrimitiveMeshes::CreateCube(m_Device);
    if (id.Str() == kBuiltinPlaneId)
        return PrimitiveMeshes::CreatePlane(m_Device);

    std::cerr << "Sapana AssetCache: unknown builtin '" << id.Str() << "'\n";
    return nullptr;
}

MeshAssetPtr AssetCache::LoadGltf(const AssetId& id)
{
    if (m_Device == nullptr || m_Context == nullptr)
    {
        std::cerr << "Sapana AssetCache: device/context required to load glTF '" << id.Str() << "'\n";
        return nullptr;
    }

    MeshAssetPtr mesh = GltfMeshLoader::Load(m_Device, m_Context, id.Str());
    if (mesh == nullptr)
        std::cerr << "Sapana AssetCache: failed to load glTF '" << id.Str() << "'\n";
    return mesh;
}

} // namespace assets
} // namespace sapana
