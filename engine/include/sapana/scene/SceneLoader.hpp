#pragma once

#include "sapana/assets/AssetCache.hpp"
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
    static bool Load(const char*                 path,
                     entt::registry&             registry,
                     assets::AssetCache&         assetCache);
};

} // namespace scene
} // namespace sapana
