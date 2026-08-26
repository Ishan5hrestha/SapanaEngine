#include "sapana/render/PbrGltfRenderer.hpp"

#include "GLTF_PBR_Renderer.hpp"
#include "GraphicsUtilities.h"
#include "MapHelper.hpp"
#include "sapana/ecs/Components.hpp"
#include "sapana/render/VisibilityAndLodSystem.hpp"

#include <iostream>
#include <unordered_map>
#include <unordered_set>

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
    LightingConfig                               Lighting{};

    Diligent::ITextureView* ShadowMapSRV      = nullptr;
    Diligent::float4x4      WorldToLightProj  = Diligent::float4x4::Identity();
    bool                    ShadowsActive     = false;
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

void PbrGltfRenderer::ApplyConfig(const LightingConfig& config)
{
    if (m_Impl == nullptr)
        return;

    m_Impl->Lighting = config;
    m_Impl->Lighting.EnsureDefaultLight();

    if (m_Impl->Renderer != nullptr)
    {
        if (m_Impl->Lighting.EnableIbl)
            m_Impl->RenderParams.Flags |= Diligent::GLTF_PBR_Renderer::PSO_FLAG_USE_IBL;
        else
            m_Impl->RenderParams.Flags &= ~Diligent::GLTF_PBR_Renderer::PSO_FLAG_USE_IBL;

        if (m_Impl->Lighting.Shadows.Enabled)
            m_Impl->RenderParams.Flags |= Diligent::GLTF_PBR_Renderer::PSO_FLAG_ENABLE_SHADOWS;
        else
            m_Impl->RenderParams.Flags &= ~Diligent::GLTF_PBR_Renderer::PSO_FLAG_ENABLE_SHADOWS;
    }
}

void PbrGltfRenderer::SetShadowResources(Diligent::ITextureView*   shadowMapSRV,
                                         const Diligent::float4x4& worldToLightProj,
                                         bool                      shadowsActive)
{
    if (m_Impl == nullptr)
        return;
    m_Impl->ShadowMapSRV     = shadowMapSRV;
    m_Impl->WorldToLightProj = worldToLightProj;
    m_Impl->ShadowsActive    = shadowsActive && shadowMapSRV != nullptr && m_Impl->Lighting.Shadows.Enabled;
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
    m_Impl->Lighting.EnsureDefaultLight();

    const bool enableShadows = m_Impl->Lighting.Shadows.Enabled;

    GLTF_PBR_Renderer::CreateInfo RendererCI;
    RendererCI.EnableIBL             = m_Impl->Lighting.EnableIbl;
    RendererCI.EnableAO              = true;
    RendererCI.EnableEmissive        = true;
    RendererCI.EnableClearCoat       = false;
    RendererCI.EnableSheen           = false;
    RendererCI.EnableAnisotropy      = false;
    RendererCI.EnableIridescence     = false;
    RendererCI.EnableTransmission    = false;
    RendererCI.EnableVolume          = false;
    RendererCI.EnableShadows         = enableShadows;
    RendererCI.CreateDefaultTextures = true;
    RendererCI.PackMatrixRowMajor    = true;
    RendererCI.MaxLightCount         = static_cast<Uint32>(kMaxConfiguredLights);
    RendererCI.MaxShadowCastingLightCount = enableShadows ? 1u : 0u;
    RendererCI.PCFKernelSize         = static_cast<Uint32>(m_Impl->Lighting.Shadows.PcfKernel);
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
    if (m_Impl->Lighting.EnableIbl)
        m_Impl->RenderParams.Flags |= GLTF_PBR_Renderer::PSO_FLAG_USE_IBL;
    else
        m_Impl->RenderParams.Flags &= ~GLTF_PBR_Renderer::PSO_FLAG_USE_IBL;
    m_Impl->RenderParams.Flags |= GLTF_PBR_Renderer::PSO_FLAG_USE_LIGHTS;
    m_Impl->RenderParams.Flags |= GLTF_PBR_Renderer::PSO_FLAG_ENABLE_TONE_MAPPING;
    if (enableShadows)
        m_Impl->RenderParams.Flags |= GLTF_PBR_Renderer::PSO_FLAG_ENABLE_SHADOWS;

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

    m_Impl->Lighting.EnsureDefaultLight();
    const LightingConfig& lighting      = m_Impl->Lighting;
    const bool            shadowsActive = m_Impl->ShadowsActive;

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

        HLSL::PBRLightAttribs* lightsOut =
            reinterpret_cast<HLSL::PBRLightAttribs*>(FrameAttribs + 1);
        HLSL::PBRShadowMapInfo* shadowMaps =
            reinterpret_cast<HLSL::PBRShadowMapInfo*>(lightsOut + kMaxConfiguredLights);

        int lightCount = 0;
        for (const LightDesc& desc : lighting.Lights)
        {
            if (lightCount >= kMaxConfiguredLights)
                break;
            if (desc.Type != LightType::Directional)
                continue;

            GLTF::Light gltfLight;
            gltfLight.Type      = GLTF::Light::TYPE::DIRECTIONAL;
            gltfLight.Color     = desc.Color;
            gltfLight.Intensity = desc.Intensity;

            const float3 lightDir = desc.Direction;
            const int    shadowIx = (shadowsActive && lightCount == 0) ? 0 : -1;
            GLTF_PBR_Renderer::WritePBRLightShaderAttribs(
                {&gltfLight, nullptr, &lightDir, 1.f, shadowIx},
                lightsOut + lightCount);
            ++lightCount;
        }

        if (lightCount == 0)
        {
            GLTF::Light fallback;
            fallback.Type      = GLTF::Light::TYPE::DIRECTIONAL;
            fallback.Color     = float3{1.f, 1.f, 1.f};
            fallback.Intensity = 3.f;
            const float3 lightDir = normalize(float3{-0.4f, -1.f, -0.3f});
            const int    shadowIx = shadowsActive ? 0 : -1;
            GLTF_PBR_Renderer::WritePBRLightShaderAttribs(
                {&fallback, nullptr, &lightDir, 1.f, shadowIx}, lightsOut);
            lightCount = 1;
        }

        if (shadowsActive && m_Impl->Renderer->GetSettings().MaxShadowCastingLightCount > 0)
        {
            shadowMaps[0].WorldToLightProjSpace = m_Impl->WorldToLightProj;
            shadowMaps[0].UVScale              = float2{1.f, 1.f};
            shadowMaps[0].UVBias               = float2{0.f, 0.f};
            shadowMaps[0].ShadowMapSlice       = 0.f;
            shadowMaps[0].Padding0 = shadowMaps[0].Padding1 = shadowMaps[0].Padding2 = 0.f;
        }

        HLSL::PBRRendererShaderParameters& renderer = FrameAttribs->Renderer;
        m_Impl->Renderer->SetInternalShaderParameters(renderer, nullptr);
        renderer.OcclusionStrength = 1.f;
        renderer.EmissionScale     = 1.f;
        renderer.AverageLogLum     = lighting.AverageLogLum;
        renderer.MiddleGray        = lighting.MiddleGray;
        renderer.WhitePoint        = lighting.WhitePoint;
        renderer.IBLScale          = lighting.EnableIbl ? float4{1.f} : float4{0};
        renderer.LightCount        = lightCount;
        renderer.DebugView         = 0;
    }

    m_Impl->Renderer->Begin(context);

    std::unordered_map<Diligent::GLTF::Model*, ModelDrawState> modelStates;
    std::unordered_set<Diligent::GLTF::Model*>                 shadowSrbsReady;

    auto viewEntities = registry.view<ecs::Transform, ecs::MeshRenderer>();
    for (auto entity : viewEntities)
    {
        if (registry.all_of<ecs::Visibility>(entity) && !registry.get<ecs::Visibility>(entity).InCamera)
            continue;

        const auto& transform = viewEntities.get<ecs::Transform>(entity);
        const auto& meshComp  = viewEntities.get<ecs::MeshRenderer>(entity);

        const assets::AssetId meshId = ResolveCameraMeshId(registry, entity, meshComp.MeshId);
        assets::MeshAssetPtr  mesh   = assetCache.GetOrLoad(meshId);
        if (mesh == nullptr || mesh->GltfModel == nullptr)
            continue;

        Diligent::GLTF::Model* model = mesh->GltfModel.get();
        ModelDrawState&        state = modelStates[model];
        if (!state.BindingsReady)
        {
            state.Bindings      = m_Impl->Renderer->CreateResourceBindings(*model, m_Impl->FrameAttribsCB);
            state.BindingsReady = true;
        }

        // Bind shadow map once per unique GLTF model (not once per entity).
        if (shadowsActive && m_Impl->ShadowMapSRV != nullptr && shadowSrbsReady.insert(model).second)
        {
            for (auto& srb : state.Bindings.MaterialSRB)
            {
                if (srb)
                {
                    m_Impl->Renderer->InitCommonSRBVars(srb, m_Impl->FrameAttribsCB, true, true, m_Impl->ShadowMapSRV);
                    context->TransitionShaderResources(srb);
                }
            }
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
