#pragma once

#include "BasicMath.hpp"
#include "DeviceContext.h"
#include "EngineFactory.h"
#include "RenderDevice.h"
#include "SwapChain.h"
#include "RefCntAutoPtr.hpp"
#include "sapana/assets/AssetCache.hpp"

#include <entt/entt.hpp>

namespace sapana
{
namespace render
{

/// Basic forward renderer: pos+color mesh, WVP + color tint constants.
/// Optional PCF shadow reception when SetShadowResources is active.
class BasicForwardRenderer
{
public:
    bool Initialize(Diligent::IRenderDevice*              device,
                    Diligent::IEngineFactory*             engineFactory,
                    Diligent::ISwapChain*                 swapChain,
                    bool                                  convertPSOutputToGamma);

    /// Bind cascade-0 shadow map for Basic receivers (e.g. ground plane).
    void SetShadowResources(Diligent::ITextureView*     shadowMapSRV,
                            const Diligent::float4x4&   worldToLightProj,
                            bool                        shadowsActive);

    void Draw(Diligent::IDeviceContext*     context,
              entt::registry&               registry,
              assets::AssetCache&           assetCache,
              const Diligent::float4x4&     viewProj,
              bool                          skipGltfBacked = false);

private:
    bool CreatePSO(Diligent::IRenderDevice*              device,
                   Diligent::IEngineFactory*             engineFactory,
                   Diligent::ISwapChain*                 swapChain,
                   bool                                  convertPSOutputToGamma,
                   bool                                  enableShadows,
                   Diligent::RefCntAutoPtr<Diligent::IPipelineState>&         outPSO,
                   Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>& outSRB);

    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_PSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_SRB;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_ShadowPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_ShadowSRB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_VSConstants;

    Diligent::ITextureView* m_ShadowMapSRV     = nullptr;
    Diligent::float4x4      m_WorldToLightProj = Diligent::float4x4::Identity();
    bool                    m_ShadowsActive    = false;
};

} // namespace render
} // namespace sapana
