#pragma once

#include "BasicMath.hpp"
#include "DeviceContext.h"
#include "EngineFactory.h"
#include "RenderDevice.h"
#include "SwapChain.h"
#include "RefCntAutoPtr.hpp"
#include "sapana/render/LightingConfig.hpp"

namespace sapana
{
namespace render
{

/// Fullscreen procedural gradient sky (view-ray). Drawn before scene geometry.
class SkyRenderer
{
public:
    bool Initialize(Diligent::IRenderDevice*  device,
                    Diligent::IEngineFactory* engineFactory,
                    Diligent::ISwapChain*     swapChain,
                    bool                      convertPSOutputToGamma);

    void ApplyConfig(const SkyConfig& config);

    void Draw(Diligent::IDeviceContext* context,
              const Diligent::float4x4& view,
              const Diligent::float4x4& proj);

    bool IsReady() const { return m_PSO && m_SRB && m_Constants; }
    bool IsEnabled() const { return m_Config.Enabled; }

private:
    SkyConfig                                             m_Config{};
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_PSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_SRB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>                m_Constants;
};

} // namespace render
} // namespace sapana
