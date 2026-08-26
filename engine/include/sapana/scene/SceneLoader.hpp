#pragma once

#include "sapana/assets/AssetCache.hpp"
#include "sapana/render/LodConfig.hpp"
#include "sapana/scene/SceneDocument.hpp"

#include <entt/entt.hpp>

namespace sapana
{
namespace scene
{

class SceneLoader
{
public:
    /// Load scene JSON into registry. Returns false on I/O or parse failure.
    /// When lodConfig is non-null, attaches LodGroup and preloads LOD1 for matching meshes.
    static bool Load(const char*              path,
                     entt::registry&          registry,
                     assets::AssetCache&      assetCache,
                     const render::LodConfig* lodConfig = nullptr);
};

} // namespace scene
} // namespace sapana
