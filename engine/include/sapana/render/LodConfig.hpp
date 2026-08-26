#pragma once

#include "sapana/assets/AssetId.hpp"

#include <string>
#include <unordered_map>

namespace sapana
{
namespace render
{

/// Per-base-mesh LOD table entry (from lod.json "meshes").
struct MeshLodEntry
{
    assets::AssetId Lod1;
    float           BoundingRadius = 2.5f;
};

/// Tunables for frustum/distance cull + mesh LOD (config/lod.json).
struct LodConfig
{
    bool  Enabled                = true;
    bool  FrustumCulling         = true;
    bool  DebugStats             = false;
    float FarCullDistance        = 70.f;
    float Lod1Distance           = 28.f;
    float Hysteresis             = 4.f;
    float ShadowFarCullDistance  = 50.f;
    float ShadowLod1Distance     = 22.f;
    float DefaultBoundingRadius  = 2.f;

    std::unordered_map<std::string, MeshLodEntry> Meshes;

    /// Load from JSON. On failure leaves *this unchanged and returns false.
    bool LoadFromFile(const char* path);

    const MeshLodEntry* FindMeshEntry(const assets::AssetId& baseId) const;
};

} // namespace render
} // namespace sapana
