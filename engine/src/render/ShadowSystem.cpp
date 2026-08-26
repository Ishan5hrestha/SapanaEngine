#include "sapana/render/ShadowSystem.hpp"

#include "CommonlyUsedStates.h"
#include "GraphicsUtilities.h"
#include "MapHelper.hpp"
#include "ShadowMapManager.hpp"
#include "sapana/ecs/Components.hpp"
#include "sapana/render/VisibilityAndLodSystem.hpp"

#include <algorithm>
#include <iostream>

namespace Diligent
{
namespace HLSL
{
#include "Shaders/Common/public/BasicStructures.fxh"
} // namespace HLSL
} // namespace Diligent

namespace sapana
{
namespace render
{

namespace
{

struct DepthConstants
{
    Diligent::float4x4 WorldViewProj;
};

} // namespace

ShadowSystem::ShadowSystem()  = default;
ShadowSystem::~ShadowSystem() = default;

void ShadowSystem::ApplyConfig(const ShadowConfig& config)
{
    m_Config = config;
    if (m_Config.Cascades < 1)
        m_Config.Cascades = 1;
    if (m_Config.Cascades > 2)
        m_Config.Cascades = 2;
    if (m_Config.Resolution != 512 && m_Config.Resolution != 1024 && m_Config.Resolution != 2048 &&
        m_Config.Resolution != 4096)
        m_Config.Resolution = 2048;
}

bool ShadowSystem::IsReady() const
{
    return m_Manager != nullptr && m_DepthPSO && m_DepthSRB && m_DepthCB && m_ComparisonSampler;
}

Diligent::ITextureView* ShadowSystem::GetShadowMapSRV() const
{
    if (m_Manager == nullptr)
        return nullptr;
    return m_Manager->GetSRV();
}

bool ShadowSystem::CreateDepthPSO(Diligent::IEngineFactory* engineFactory, Diligent::ISwapChain* /*swapChain*/)
{
    using namespace Diligent;

    if (m_Device == nullptr || engineFactory == nullptr)
        return false;

    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name                       = "Sapana Shadow Depth PSO";
    PSOCreateInfo.PSODesc.PipelineType               = PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets  = 0;
    PSOCreateInfo.GraphicsPipeline.DSVFormat          = TEX_FORMAT_D32_FLOAT;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthClipEnable = False; // allow depth clamp if supported
    // Reduce shadow acne on receivers (ground / PBR meshes).
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.DepthBias            = 2;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.SlopeScaledDepthBias = 3.5f;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = True;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS_EQUAL;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.CompileFlags   = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    engineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Sapana Shadow Depth VS";
        ShaderCI.FilePath        = "shaders/shadow_depth.vsh";
        m_Device->CreateShader(ShaderCI, &pVS);
        if (!pVS)
        {
            std::cerr << "Sapana ShadowSystem: failed to create depth VS\n";
            return false;
        }
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Sapana Shadow Depth PS";
        ShaderCI.FilePath        = "shaders/shadow_depth.psh";
        m_Device->CreateShader(ShaderCI, &pPS);
        if (!pPS)
        {
            std::cerr << "Sapana ShadowSystem: failed to create depth PS\n";
            return false;
        }
    }

    BufferDesc CBDesc;
    CBDesc.Name           = "Sapana Shadow Depth CB";
    CBDesc.Size           = sizeof(DepthConstants);
    CBDesc.Usage          = USAGE_DYNAMIC;
    CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
    CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    m_Device->CreateBuffer(CBDesc, nullptr, &m_DepthCB);
    if (!m_DepthCB)
        return false;

    LayoutElement LayoutElems[] = {
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        LayoutElement{1, 0, 4, VT_FLOAT32, False}};
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);
    PSOCreateInfo.pVS                                         = pVS;
    PSOCreateInfo.pPS                                         = pPS;
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType  = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    m_Device->CreateGraphicsPipelineState(PSOCreateInfo, &m_DepthPSO);
    if (!m_DepthPSO)
    {
        std::cerr << "Sapana ShadowSystem: failed to create depth PSO\n";
        return false;
    }

    m_DepthPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_DepthCB);
    m_DepthPSO->CreateShaderResourceBinding(&m_DepthSRB, true);
    return m_DepthSRB != nullptr;
}

bool ShadowSystem::CreateShadowMap()
{
    using namespace Diligent;

    if (m_Device == nullptr)
        return false;

    if (!m_ComparisonSampler)
    {
        m_Device->CreateSampler(Sam_ComparisonLinearClamp, &m_ComparisonSampler);
        if (!m_ComparisonSampler)
        {
            std::cerr << "Sapana ShadowSystem: failed to create comparison sampler\n";
            return false;
        }
    }

    m_Manager = std::make_unique<ShadowMapManager>();

    ShadowMapManager::InitInfo init;
    init.Format               = TEX_FORMAT_D32_FLOAT;
    init.Resolution           = static_cast<Uint32>(m_Config.Resolution);
    init.NumCascades          = static_cast<Uint32>(m_Config.Cascades);
    init.ShadowMode           = SHADOW_MODE_PCF;
    init.pComparisonSampler   = m_ComparisonSampler;

    try
    {
        m_Manager->Initialize(m_Device, nullptr, init);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Sapana ShadowSystem: ShadowMapManager init failed: " << ex.what() << '\n';
        m_Manager.reset();
        return false;
    }
    catch (...)
    {
        std::cerr << "Sapana ShadowSystem: ShadowMapManager init failed\n";
        m_Manager.reset();
        return false;
    }

    if (m_Manager->GetSRV() == nullptr)
    {
        std::cerr << "Sapana ShadowSystem: shadow map SRV is null\n";
        m_Manager.reset();
        return false;
    }

    return true;
}

bool ShadowSystem::Initialize(Diligent::IRenderDevice*  device,
                              Diligent::IEngineFactory* engineFactory,
                              Diligent::ISwapChain*     swapChain)
{
    if (device == nullptr || engineFactory == nullptr || swapChain == nullptr)
        return false;

    m_Device              = device;
    m_PackMatrixRowMajor  = !device->GetDeviceInfo().IsGLDevice();

    if (!CreateDepthPSO(engineFactory, swapChain))
        return false;

    if (!m_Config.Enabled)
        return true; // depth PSO ready; map created lazily when enabled

    return CreateShadowMap();
}

void ShadowSystem::Update(const Diligent::float4x4& cameraView,
                          const Diligent::float4x4& cameraProj,
                          const Diligent::float3&   lightDirection,
                          float                     cameraNear,
                          float                     cameraFar)
{
    using namespace Diligent;
    using namespace HLSL;

    if (!IsEnabled() || m_Manager == nullptr)
        return;

    const float3 lightDir = normalize(lightDirection);
    const float  maxZ     = std::min(cameraFar, m_Config.MaxDistance);
    const float  minZ     = cameraNear;

    ShadowMapAttribs shadowAttribs{};
    shadowAttribs.iNumCascades    = m_Config.Cascades;
    shadowAttribs.fFixedDepthBias = m_Config.DepthBias;
    shadowAttribs.iFixedFilterSize = m_Config.PcfKernel;

    ShadowMapManager::DistributeCascadeInfo distr;
    distr.pCameraView                 = &cameraView;
    distr.pCameraProj                 = &cameraProj;
    distr.pLightDir                   = &lightDir;
    distr.fPartitioningFactor         = 0.95f;
    distr.SnapCascades                = true;
    distr.StabilizeExtents            = true;
    distr.EqualizeExtents             = true;
    distr.UseRightHandedLightViewTransform = false; // Diligent PBR / DX-style
    distr.PackMatrixRowMajor          = m_PackMatrixRowMajor;
    distr.AdjustCascadeRange = [minZ, maxZ](int cascadeIdx, float& zNear, float& zFar) {
        if (cascadeIdx < 0)
        {
            zNear = minZ;
            zFar  = maxZ;
        }
        else
        {
            zNear = std::max(zNear, minZ);
            zFar  = std::min(zFar, maxZ);
        }
    };

    m_Manager->DistributeCascades(distr, shadowAttribs);
    m_WorldToLightProj = m_Manager->GetCascadeTransform(0).WorldToLightProjSpace;
}

void ShadowSystem::RenderCasters(Diligent::IDeviceContext* context,
                                 entt::registry&           registry,
                                 assets::AssetCache&       assetCache)
{
    using namespace Diligent;

    if (context == nullptr || !IsEnabled() || m_Manager == nullptr || !m_DepthPSO)
        return;

    const Uint32 cascadeCount = static_cast<Uint32>(m_Config.Cascades);
    for (Uint32 cascade = 0; cascade < cascadeCount; ++cascade)
    {
        ITextureView* pDSV = m_Manager->GetCascadeDSV(cascade);
        if (pDSV == nullptr)
            continue;

        context->SetRenderTargets(0, nullptr, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->SetPipelineState(m_DepthPSO);
        context->CommitShaderResources(m_DepthSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const float4x4& lightVP = m_Manager->GetCascadeTransform(cascade).WorldToLightProjSpace;

        auto view = registry.view<ecs::Transform, ecs::MeshRenderer>();
        for (auto entity : view)
        {
            if (registry.all_of<ecs::Visibility>(entity) && !registry.get<ecs::Visibility>(entity).InShadow)
                continue;

            const auto& transform = view.get<ecs::Transform>(entity);
            const auto& renderer  = view.get<ecs::MeshRenderer>(entity);

            const assets::AssetId meshId = ResolveShadowMeshId(registry, entity, renderer.MeshId);
            assets::MeshAssetPtr  mesh   = assetCache.GetOrLoad(meshId);
            if (mesh == nullptr || !mesh->VertexBuffer || !mesh->IndexBuffer || mesh->IndexCount == 0)
                continue;

            {
                MapHelper<DepthConstants> cb{context, m_DepthCB, MAP_WRITE, MAP_FLAG_DISCARD};
                cb->WorldViewProj = transform.ToMatrix() * lightVP;
            }

            IBuffer*     pBuffs[] = {mesh->VertexBuffer};
            const Uint64 offset   = 0;
            context->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                      SET_VERTEX_BUFFERS_FLAG_RESET);
            context->SetIndexBuffer(mesh->IndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            DrawIndexedAttribs drawAttrs;
            drawAttrs.IndexType  = mesh->IndexType;
            drawAttrs.NumIndices = mesh->IndexCount;
            drawAttrs.Flags      = DRAW_FLAG_VERIFY_ALL;
            context->DrawIndexed(drawAttrs);
        }
    }

    // Unbind cascade DSVs, then make the map readable for PBR (VERIFY commits).
    context->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
    if (ITextureView* pSRV = m_Manager->GetSRV())
    {
        ITexture* pTex = pSRV->GetTexture();
        if (pTex != nullptr)
        {
            const RenderDeviceInfo& deviceInfo = m_Device->GetDeviceInfo();
            const RESOURCE_STATE    sampleState =
                deviceInfo.IsD3DDevice() ? RESOURCE_STATE_SHADER_RESOURCE : RESOURCE_STATE_DEPTH_READ;
            StateTransitionDesc barrier{pTex, RESOURCE_STATE_UNKNOWN, sampleState,
                                        STATE_TRANSITION_FLAG_UPDATE_STATE};
            context->TransitionResourceStates(1, &barrier);
        }
    }
}

} // namespace render
} // namespace sapana
