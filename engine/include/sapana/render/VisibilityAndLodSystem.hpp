#pragma once

#include "BasicMath.hpp"
#include "sapana/assets/AssetId.hpp"
#include "sapana/render/LodConfig.hpp"

#include <entt/entt.hpp>

namespace sapana
{
namespace render
{

struct LodFrameStats
{
    int TotalMeshEntities = 0;
    int InCamera          = 0;
    int InShadow          = 0;
    int Lod0              = 0;
    int Lod1              = 0;
    int CulledCamera      = 0;
    int CulledShadow      = 0;
};

/// Updates Visibility + LodGroup levels from camera pose / view-proj each frame.
class VisibilityAndLodSystem
{
public:
    void ApplyConfig(const LodConfig& config);

    /// cameraPos: world-space eye. viewProj: camera view * projection. isGL: NDC Z convention.
    void Update(entt::registry&           registry,
                const Diligent::float3&   cameraPos,
                const Diligent::float4x4& viewProj,
                bool                      isGL);

    const LodFrameStats& GetStats() const { return m_Stats; }

private:
    LodConfig     m_Config{};
    LodFrameStats m_Stats{};
};

/// Resolve which mesh asset to draw for the color pass (camera LOD).
assets::AssetId ResolveCameraMeshId(const entt::registry&     registry,
                                    entt::entity              entity,
                                    const assets::AssetId&    baseMeshId);

/// Resolve which mesh asset to draw for the shadow caster pass.
assets::AssetId ResolveShadowMeshId(const entt::registry&     registry,
                                    entt::entity              entity,
                                    const assets::AssetId&    baseMeshId);

} // namespace render
} // namespace sapana
