#pragma once

#include "BasicMath.hpp"
#include "DeviceContext.h"
#include "RenderDevice.h"
#include "RefCntAutoPtr.hpp"
#include "SwapChain.h"
#include "sapana/assets/AssetCache.hpp"

#include <entt/entt.hpp>
#include <memory>

namespace Diligent
{
class GLTF_PBR_Renderer;
} // namespace Diligent

namespace sapana
{
namespace render
{

/// Minimal DiligentFX GLTF_PBR_Renderer wrapper for glTF-backed entities.
/// Builtins without a GltfModel are skipped (caller should draw them via Basic).
class PbrGltfRenderer
{
public:
    PbrGltfRenderer();
    ~PbrGltfRenderer();

    PbrGltfRenderer(const PbrGltfRenderer&)            = delete;
    PbrGltfRenderer& operator=(const PbrGltfRenderer&) = delete;

    bool Initialize(Diligent::IRenderDevice*  device,
                    Diligent::IDeviceContext* context,
                    Diligent::ISwapChain*     swapChain,
                    bool                      convertPSOutputToGamma);

    /// Draws entities that have a MeshRenderer whose mesh owns a GLTF::Model.
    void Draw(Diligent::IDeviceContext* context,
              entt::registry&           registry,
              assets::AssetCache&       assetCache,
              const Diligent::float4x4& view,
              const Diligent::float4x4& proj,
              const Diligent::float4x4& viewProj,
              Diligent::Uint32          viewportWidth,
              Diligent::Uint32          viewportHeight);

    bool IsReady() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace render
} // namespace sapana
