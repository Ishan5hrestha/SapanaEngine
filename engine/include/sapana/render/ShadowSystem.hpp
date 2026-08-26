#pragma once

#include "BasicMath.hpp"
#include "DeviceContext.h"
#include "EngineFactory.h"
#include "RenderDevice.h"
#include "SwapChain.h"
#include "RefCntAutoPtr.hpp"
#include "sapana/assets/AssetCache.hpp"
#include "sapana/render/LightingConfig.hpp"

#include <entt/entt.hpp>
#include <memory>

namespace Diligent
{
class ShadowMapManager;
} // namespace Diligent

namespace sapana
{
namespace render
{

/// Owns DiligentFX ShadowMapManager + depth-only caster draw for the primary sun.
class ShadowSystem
{
public:
    ShadowSystem();
    ~ShadowSystem();

    ShadowSystem(const ShadowSystem&)            = delete;
    ShadowSystem& operator=(const ShadowSystem&) = delete;

    void ApplyConfig(const ShadowConfig& config);

    bool Initialize(Diligent::IRenderDevice*  device,
                    Diligent::IEngineFactory* engineFactory,
                    Diligent::ISwapChain*     swapChain);

    bool IsReady() const;
    bool IsEnabled() const { return m_Config.Enabled && IsReady(); }

    /// Distribute cascades for this frame using camera + light direction (toward scene).
    void Update(const Diligent::float4x4& cameraView,
                const Diligent::float4x4& cameraProj,
                const Diligent::float3&   lightDirection,
                float                     cameraNear,
                float                     cameraFar);

    /// Render depth casters into all cascades (all MeshAssets with Basic VB/IB).
    void RenderCasters(Diligent::IDeviceContext* context,
                       entt::registry&           registry,
                       assets::AssetCache&       assetCache);

    Diligent::ITextureView* GetShadowMapSRV() const;

    /// World-to-light-proj for cascade 0 (used by PBR PBRShadowMapInfo).
    const Diligent::float4x4& GetWorldToLightProj() const { return m_WorldToLightProj; }

    float GetDepthBias() const { return m_Config.DepthBias; }
    int   GetPcfKernel() const { return m_Config.PcfKernel; }

private:
    bool CreateShadowMap();
    bool CreateDepthPSO(Diligent::IEngineFactory* engineFactory, Diligent::ISwapChain* swapChain);

    ShadowConfig                                   m_Config{};
    Diligent::IRenderDevice*                       m_Device = nullptr;
    std::unique_ptr<Diligent::ShadowMapManager>    m_Manager;
    Diligent::RefCntAutoPtr<Diligent::ISampler>    m_ComparisonSampler;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_DepthPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_DepthSRB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_DepthCB;
    Diligent::float4x4                             m_WorldToLightProj = Diligent::float4x4::Identity();
    bool                                           m_PackMatrixRowMajor = true;
};

} // namespace render
} // namespace sapana
