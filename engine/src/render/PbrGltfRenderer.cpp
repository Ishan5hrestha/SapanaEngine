#include "sapana/render/PbrGltfRenderer.hpp"

#include "GLTF_PBR_Renderer.hpp"
#include "GraphicsUtilities.h"
#include "MapHelper.hpp"
#include "sapana/ecs/Components.hpp"

#include <iostream>
#include <unordered_map>

namespace Diligent
{
namespace HLSL
{
#include "Shaders/Common/public/BasicStructures.fxh"
#include "Shaders/PBR/public/PBR_Structures.fxh"
#include "Shaders/PBR/private/RenderPBR_Structures.fxh"
} // namespace HLSL
} // namespace Diligent

namespace sapana
{
namespace render
{

struct PbrGltfRenderer::Impl
{
    Diligent::IRenderDevice*                     Device = nullptr;
    std::unique_ptr<Diligent::GLTF_PBR_Renderer> Renderer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer>   FrameAttribsCB;
    Diligent::GLTF_PBR_Renderer::RenderInfo      RenderParams{};
};

namespace
{

struct ModelDrawState
{
    Diligent::GLTF::ModelTransforms                    Transforms;
    Diligent::GLTF_PBR_Renderer::ModelResourceBindings Bindings;
    bool                                               BindingsReady = false;
};

} // namespace

PbrGltfRenderer::PbrGltfRenderer()
    : m_Impl(std::make_unique<Impl>())
{
}

PbrGltfRenderer::~PbrGltfRenderer() = default;

bool PbrGltfRenderer::IsReady() const
{
    return m_Impl != nullptr && m_Impl->Renderer != nullptr && m_Impl->FrameAttribsCB;
}

bool PbrGltfRenderer::Initialize(Diligent::IRenderDevice*  device,
                                 Diligent::IDeviceContext* context,
                                 Diligent::ISwapChain*     swapChain,
                                 bool                      convertPSOutputToGamma)
{
    using namespace Diligent;

    if (device == nullptr || context == nullptr || swapChain == nullptr || m_Impl == nullptr)
        return false;

    m_Impl->Device = device;

    GLTF_PBR_Renderer::CreateInfo RendererCI;
    // Keep the path light for Intel HD 620 / fast iteration; IBL opt-in later.
    RendererCI.EnableIBL             = false;
    RendererCI.EnableAO              = true;
    RendererCI.EnableEmissive        = true;
    RendererCI.EnableClearCoat       = false;
    RendererCI.EnableSheen           = false;
    RendererCI.EnableAnisotropy      = false;
    RendererCI.EnableIridescence     = false;
    RendererCI.EnableTransmission    = false;
    RendererCI.EnableVolume          = false;
    RendererCI.CreateDefaultTextures = true;
    RendererCI.PackMatrixRowMajor    = true;
    RendererCI.MaxLightCount         = 4;
    RendererCI.NumRenderTargets      = 1;
    RendererCI.RTVFormats[0]         = swapChain->GetDesc().ColorBufferFormat;
    RendererCI.DSVFormat             = swapChain->GetDesc().DepthBufferFormat;

    try
    {
        m_Impl->Renderer = std::make_unique<GLTF_PBR_Renderer>(device, nullptr, context, RendererCI);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Sapana PbrGltfRenderer: failed to create GLTF_PBR_Renderer: " << ex.what() << '\n';
        return false;
    }
    catch (...)
    {
        std::cerr << "Sapana PbrGltfRenderer: failed to create GLTF_PBR_Renderer\n";
        return false;
    }

    CreateUniformBuffer(device, m_Impl->Renderer->GetPRBFrameAttribsSize(), "Sapana PBR frame attribs", &m_Impl->FrameAttribsCB);
    if (!m_Impl->FrameAttribsCB)
    {
        std::cerr << "Sapana PbrGltfRenderer: failed to create frame attribs CB\n";
        m_Impl->Renderer.reset();
        return false;
    }

    {
        StateTransitionDesc Barriers[] = {
            {m_Impl->FrameAttribsCB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        };
        context->TransitionResourceStates(_countof(Barriers), Barriers);
    }

    m_Impl->RenderParams.Flags = GLTF_PBR_Renderer::PSO_FLAG_DEFAULT;
    m_Impl->RenderParams.Flags &= ~GLTF_PBR_Renderer::PSO_FLAG_USE_IBL;
    m_Impl->RenderParams.Flags |= GLTF_PBR_Renderer::PSO_FLAG_USE_LIGHTS;
    m_Impl->RenderParams.Flags |= GLTF_PBR_Renderer::PSO_FLAG_ENABLE_TONE_MAPPING;

    if (convertPSOutputToGamma ||
        RendererCI.RTVFormats[0] == TEX_FORMAT_RGBA8_UNORM ||
        RendererCI.RTVFormats[0] == TEX_FORMAT_BGRA8_UNORM)
    {
        m_Impl->RenderParams.Flags |= GLTF_PBR_Renderer::PSO_FLAG_CONVERT_OUTPUT_TO_SRGB;
    }

    return true;
}

void PbrGltfRenderer::Draw(Diligent::IDeviceContext* context,
                           entt::registry&           registry,
                           assets::AssetCache&       assetCache,
                           const Diligent::float4x4& view,
                           const Diligent::float4x4& proj,
                           const Diligent::float4x4& viewProj,
                           Diligent::Uint32          viewportWidth,
                           Diligent::Uint32          viewportHeight)
{
    using namespace Diligent;

    if (context == nullptr || !IsReady() || viewportWidth == 0 || viewportHeight == 0)
        return;

    {
        MapHelper<HLSL::PBRFrameAttribs> FrameAttribs{context, m_Impl->FrameAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD};

        const float4x4 viewInv     = view.Inverse();
        const float4x4 projInv     = proj.Inverse();
        const float4x4 viewProjInv = viewProj.Inverse();
        const float3   camPos      = float3::MakeVector(viewInv[3]);

        HLSL::CameraAttribs& cam = FrameAttribs->Camera;
        cam.f4ViewportSize       = float4{
            static_cast<float>(viewportWidth),
            static_cast<float>(viewportHeight),
            1.f / static_cast<float>(viewportWidth),
            1.f / static_cast<float>(viewportHeight)};
        cam.SetClipPlanes(0.1f, 1000.f);
        cam.fHandness    = view.Determinant() > 0 ? 1.f : -1.f;
        cam.mView        = view;
        cam.mProj        = proj;
        cam.mViewProj    = viewProj;
        cam.mViewInv     = viewInv;
        cam.mProjInv     = projInv;
        cam.mViewProjInv = viewProjInv;
        cam.f4Position   = float4(camPos, 1.f);
        FrameAttribs->PrevCamera = cam;

        GLTF::Light defaultLight;
        defaultLight.Type      = GLTF::Light::TYPE::DIRECTIONAL;
        defaultLight.Color     = float3{1.f, 1.f, 1.f};
        defaultLight.Intensity = 3.f;
        const float3 lightDir  = normalize(float3{-0.4f, -1.f, -0.3f});

        HLSL::PBRLightAttribs* lights = reinterpret_cast<HLSL::PBRLightAttribs*>(FrameAttribs + 1);
        GLTF_PBR_Renderer::WritePBRLightShaderAttribs({&defaultLight, nullptr, &lightDir, 1.f}, lights);

        HLSL::PBRRendererShaderParameters& renderer = FrameAttribs->Renderer;
        m_Impl->Renderer->SetInternalShaderParameters(renderer, nullptr);
        renderer.OcclusionStrength = 1.f;
        renderer.EmissionScale     = 1.f;
        renderer.AverageLogLum     = 0.3f;
        renderer.MiddleGray        = 0.18f;
        renderer.WhitePoint        = 3.f;
        renderer.IBLScale          = float4{0};
        renderer.LightCount        = 1;
        renderer.DebugView         = 0;
    }

    m_Impl->Renderer->Begin(context);

    std::unordered_map<Diligent::GLTF::Model*, ModelDrawState> modelStates;

    auto viewEntities = registry.view<ecs::Transform, ecs::MeshRenderer>();
    for (auto entity : viewEntities)
    {
        const auto& transform = viewEntities.get<ecs::Transform>(entity);
        const auto& meshComp  = viewEntities.get<ecs::MeshRenderer>(entity);

        assets::MeshAssetPtr mesh = assetCache.GetOrLoad(meshComp.MeshId);
        if (mesh == nullptr || mesh->GltfModel == nullptr)
            continue;

        Diligent::GLTF::Model* model = mesh->GltfModel.get();
        ModelDrawState&        state = modelStates[model];
        if (!state.BindingsReady)
        {
            state.Bindings      = m_Impl->Renderer->CreateResourceBindings(*model, m_Impl->FrameAttribsCB);
            state.BindingsReady = true;
        }

        model->ComputeTransforms(model->DefaultSceneId, state.Transforms, transform.ToMatrix());

        Diligent::GLTF_PBR_Renderer::RenderInfo params = m_Impl->RenderParams;
        params.SceneIndex                              = static_cast<Diligent::Uint32>(model->DefaultSceneId);
        params.ModelTransform                          = Diligent::float4x4::Identity();

        m_Impl->Renderer->Render(context, *model, state.Transforms, &state.Transforms, params, &state.Bindings);
    }
}

} // namespace render
} // namespace sapana
