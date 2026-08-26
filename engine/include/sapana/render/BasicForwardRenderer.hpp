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
class BasicForwardRenderer
{
public:
    bool Initialize(Diligent::IRenderDevice*              device,
                    Diligent::IEngineFactory*             engineFactory,
                    Diligent::ISwapChain*                 swapChain,
                    bool                                  convertPSOutputToGamma);

    void Draw(Diligent::IDeviceContext*     context,
              entt::registry&               registry,
              assets::AssetCache&           assetCache,
              const Diligent::float4x4&     viewProj,
              bool                          skipGltfBacked = false);

private:
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_PSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_SRB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_VSConstants;
};

} // namespace render
} // namespace sapana
