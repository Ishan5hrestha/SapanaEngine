#include "sapana/render/VisibilityAndLodSystem.hpp"

#include "AdvancedMath.hpp"
#include "sapana/ecs/Components.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace sapana
{
namespace render
{

namespace
{

using Diligent::BoundBox;
using Diligent::BoxVisibility;
using Diligent::ExtractViewFrustumPlanesFromMatrix;
using Diligent::float3;
using Diligent::float4x4;
using Diligent::FRUSTUM_PLANE_FLAG_FULL_FRUSTUM;
using Diligent::FRUSTUM_PLANE_FLAG_OPEN_NEAR;
using Diligent::GetBoxVisibility;
using Diligent::ViewFrustum;

/// World-space bounding sphere radius from local radius and non-uniform scale.
float WorldRadius(float localRadius, const float3& scale)
{
    const float3 extents{localRadius * std::abs(scale.x), localRadius * std::abs(scale.y),
                         localRadius * std::abs(scale.z)};
    return length(extents);
}

BoundBox SphereBounds(const float3& center, float radius)
{
    BoundBox box;
    box.Min = center - float3{radius, radius, radius};
    box.Max = center + float3{radius, radius, radius};
    return box;
}

int SelectLodLevel(int current, float distance, float switchDistance, float hysteresis)
{
    if (switchDistance <= 0.f)
        return 0;

    if (current <= 0)
    {
        // Switch to LOD1 when clearly beyond threshold + hysteresis.
        return (distance > switchDistance + hysteresis) ? 1 : 0;
    }

    // Stick on LOD1 until clearly closer than threshold - hysteresis.
    return (distance < switchDistance - hysteresis) ? 0 : 1;
}

/// Sticky far cull on distance-to-center only (do NOT add mesh radius — large
/// ground planes would vanish almost immediately).
bool StickyFarVisible(bool wasVisible, float distance, float cullDistance, float hysteresis)
{
    if (cullDistance <= 0.f)
        return true;

    const float h = std::max(hysteresis, 0.f);
    if (wasVisible)
        return distance <= (cullDistance + h);
    return distance < (cullDistance - h);
}

} // namespace

void VisibilityAndLodSystem::ApplyConfig(const LodConfig& config)
{
    m_Config = config;
}

assets::AssetId ResolveCameraMeshId(const entt::registry&  registry,
                                    entt::entity           entity,
                                    const assets::AssetId& baseMeshId)
{
    if (registry.all_of<ecs::LodGroup>(entity))
    {
        const auto& lod = registry.get<ecs::LodGroup>(entity);
        if (lod.CurrentLevel >= 1 && !lod.Lod1MeshId.Empty())
            return lod.Lod1MeshId;
        if (!lod.BaseMeshId.Empty())
            return lod.BaseMeshId;
    }
    return baseMeshId;
}

assets::AssetId ResolveShadowMeshId(const entt::registry&  registry,
                                    entt::entity           entity,
                                    const assets::AssetId& baseMeshId)
{
    if (registry.all_of<ecs::LodGroup>(entity))
    {
        const auto& lod = registry.get<ecs::LodGroup>(entity);
        if (lod.ShadowLevel >= 1 && !lod.Lod1MeshId.Empty())
            return lod.Lod1MeshId;
        if (!lod.BaseMeshId.Empty())
            return lod.BaseMeshId;
    }
    return baseMeshId;
}

void VisibilityAndLodSystem::Update(entt::registry&           registry,
                                    const float3&             cameraPos,
                                    const float4x4&           viewProj,
                                    bool                      isGL)
{
    m_Stats = LodFrameStats{};

    ViewFrustum frustum{};
    if (m_Config.Enabled && m_Config.FrustumCulling)
        ExtractViewFrustumPlanesFromMatrix(viewProj, frustum, isGL);

    auto view = registry.view<ecs::Transform, ecs::MeshRenderer>();
    for (auto entity : view)
    {
        ++m_Stats.TotalMeshEntities;

        const auto& transform = view.get<ecs::Transform>(entity);
        const auto& renderer  = view.get<ecs::MeshRenderer>(entity);

        float localRadius = m_Config.DefaultBoundingRadius;
        if (registry.all_of<ecs::LodGroup>(entity))
            localRadius = registry.get<ecs::LodGroup>(entity).BoundingRadius;

        const float  radius   = WorldRadius(localRadius, transform.Scale);
        const float3 center   = transform.Position;
        const float3 delta    = center - cameraPos;
        const float  distance = length(delta);

        // Previous sticky flags (default visible if component missing).
        bool prevInCamera = true;
        bool prevInShadow = true;
        if (registry.all_of<ecs::Visibility>(entity))
        {
            const auto& prev = registry.get<ecs::Visibility>(entity);
            prevInCamera     = prev.InCamera;
            prevInShadow     = prev.InShadow;
        }

        ecs::Visibility vis{};
        vis.InCamera = true;
        vis.InShadow = true;

        if (m_Config.Enabled)
        {
            vis.InCamera = StickyFarVisible(prevInCamera, distance, m_Config.FarCullDistance,
                                            m_Config.Hysteresis);
            vis.InShadow = StickyFarVisible(prevInShadow, distance, m_Config.ShadowFarCullDistance,
                                            m_Config.Hysteresis);

            if (m_Config.FrustumCulling && (vis.InCamera || vis.InShadow))
            {
                const BoundBox box = SphereBounds(center, radius);
                if (vis.InCamera)
                {
                    const BoxVisibility camVis =
                        GetBoxVisibility(frustum, box, FRUSTUM_PLANE_FLAG_FULL_FRUSTUM);
                    if (camVis == BoxVisibility::Invisible)
                        vis.InCamera = false;
                }
                if (vis.InShadow)
                {
                    // Open near so casters slightly behind the near plane still cast.
                    const BoxVisibility shadowVis =
                        GetBoxVisibility(frustum, box, FRUSTUM_PLANE_FLAG_OPEN_NEAR);
                    if (shadowVis == BoxVisibility::Invisible)
                        vis.InShadow = false;
                }
            }
        }

        if (registry.all_of<ecs::Visibility>(entity))
            registry.replace<ecs::Visibility>(entity, vis);
        else
            registry.emplace<ecs::Visibility>(entity, vis);

        if (vis.InCamera)
            ++m_Stats.InCamera;
        else
            ++m_Stats.CulledCamera;
        if (vis.InShadow)
            ++m_Stats.InShadow;
        else
            ++m_Stats.CulledShadow;

        if (registry.all_of<ecs::LodGroup>(entity))
        {
            auto& lod = registry.get<ecs::LodGroup>(entity);
            if (m_Config.Enabled && !lod.Lod1MeshId.Empty())
            {
                lod.CurrentLevel =
                    SelectLodLevel(lod.CurrentLevel, distance, m_Config.Lod1Distance, m_Config.Hysteresis);
                lod.ShadowLevel =
                    SelectLodLevel(lod.ShadowLevel, distance, m_Config.ShadowLod1Distance, m_Config.Hysteresis);
            }
            else
            {
                lod.CurrentLevel = 0;
                lod.ShadowLevel  = 0;
            }

            if (lod.CurrentLevel >= 1)
                ++m_Stats.Lod1;
            else
                ++m_Stats.Lod0;
        }
        else
        {
            ++m_Stats.Lod0;
        }

        (void)renderer;
    }

    if (m_Config.DebugStats)
    {
        static int s_Frames = 0;
        ++s_Frames;
        if (s_Frames >= 60)
        {
            s_Frames = 0;
            std::cerr << "Sapana LOD: total=" << m_Stats.TotalMeshEntities
                      << " cam=" << m_Stats.InCamera << " (culled " << m_Stats.CulledCamera << ")"
                      << " shadow=" << m_Stats.InShadow << " (culled " << m_Stats.CulledShadow << ")"
                      << " lod0=" << m_Stats.Lod0 << " lod1=" << m_Stats.Lod1 << '\n';
        }
    }
}

} // namespace render
} // namespace sapana
