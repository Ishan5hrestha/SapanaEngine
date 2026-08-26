#include "sapana/render/BasicForwardRenderer.hpp"

#include "MapHelper.hpp"
#include "sapana/ecs/Components.hpp"
#include "sapana/render/VisibilityAndLodSystem.hpp"

#include <cstring>
#include <iostream>

namespace sapana
{
namespace render
{

namespace
{

struct BasicVSConstants
{
    Diligent::float4x4 WorldViewProj;
    Diligent::float4x4 World;
    Diligent::float4x4 WorldToLightProj;
    Diligent::float4   ColorTint;
};

} // namespace

bool BasicForwardRenderer::CreatePSO(Diligent::IRenderDevice*              device,
                                     Diligent::IEngineFactory*             engineFactory,
                                     Diligent::ISwapChain*                 swapChain,
                                     bool                                  convertPSOutputToGamma,
                                     bool                                  enableShadows,
                                     Diligent::RefCntAutoPtr<Diligent::IPipelineState>&         outPSO,
                                     Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>& outSRB)
{
    using namespace Diligent;

    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name                      = enableShadows ? "Sapana Basic Forward Shadow PSO" : "Sapana Basic Forward PSO";
    PSOCreateInfo.PSODesc.PipelineType              = PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]    = swapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat        = swapChain->GetDesc().DepthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderCI.CompileFlags                    = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    ShaderMacro Macros[] = {
        {"CONVERT_PS_OUTPUT_TO_GAMMA", convertPSOutputToGamma ? "1" : "0"},
        {"ENABLE_BASIC_SHADOWS", enableShadows ? "1" : "0"}};
    ShaderCI.Macros = {Macros, _countof(Macros)};

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    engineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = enableShadows ? "Sapana Basic Shadow VS" : "Sapana Basic VS";
        ShaderCI.FilePath        = "cube.vsh";
        device->CreateShader(ShaderCI, &pVS);
        if (!pVS)
        {
            std::cerr << "Sapana BasicForwardRenderer: failed to create VS\n";
            return false;
        }
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = enableShadows ? "Sapana Basic Shadow PS" : "Sapana Basic PS";
        ShaderCI.FilePath        = "cube.psh";
        device->CreateShader(ShaderCI, &pPS);
        if (!pPS)
        {
            std::cerr << "Sapana BasicForwardRenderer: failed to create PS\n";
            return false;
        }
    }

    LayoutElement LayoutElems[] = {
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        LayoutElement{1, 0, 4, VT_FLOAT32, False}};
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);
    PSOCreateInfo.pVS                                         = pVS;
    PSOCreateInfo.pPS                                         = pPS;

    if (enableShadows)
    {
        ShaderResourceVariableDesc Vars[] = {
            {SHADER_TYPE_PIXEL, "g_ShadowMap", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
        PSOCreateInfo.PSODesc.ResourceLayout.Variables    = Vars;
        PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = _countof(Vars);
        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    }
    else
    {
        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;
    }

    device->CreateGraphicsPipelineState(PSOCreateInfo, &outPSO);
    if (!outPSO)
    {
        std::cerr << "Sapana BasicForwardRenderer: failed to create PSO\n";
        return false;
    }

    outPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    if (enableShadows)
    {
        if (auto* pPSConstants = outPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Constants"))
            pPSConstants->Set(m_VSConstants);
    }
    outPSO->CreateShaderResourceBinding(&outSRB, true);
    return outSRB != nullptr;
}

bool BasicForwardRenderer::Initialize(Diligent::IRenderDevice*  device,
                                      Diligent::IEngineFactory* engineFactory,
                                      Diligent::ISwapChain*     swapChain,
                                      bool                      convertPSOutputToGamma)
{
    using namespace Diligent;

    if (device == nullptr || engineFactory == nullptr || swapChain == nullptr)
        return false;

    BufferDesc CBDesc;
    CBDesc.Name           = "Sapana Basic VS CB";
    CBDesc.Size           = sizeof(BasicVSConstants);
    CBDesc.Usage          = USAGE_DYNAMIC;
    CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
    CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    device->CreateBuffer(CBDesc, nullptr, &m_VSConstants);
    if (!m_VSConstants)
        return false;

    if (!CreatePSO(device, engineFactory, swapChain, convertPSOutputToGamma, false, m_PSO, m_SRB))
        return false;

    // Shadow-receiving PSO is optional; Basic still works without it.
    if (!CreatePSO(device, engineFactory, swapChain, convertPSOutputToGamma, true, m_ShadowPSO, m_ShadowSRB))
    {
        std::cerr << "Sapana BasicForwardRenderer: shadow PSO unavailable (Basic receivers unshadowed)\n";
        m_ShadowPSO.Release();
        m_ShadowSRB.Release();
    }

    return true;
}

void BasicForwardRenderer::SetShadowResources(Diligent::ITextureView*   shadowMapSRV,
                                              const Diligent::float4x4& worldToLightProj,
                                              bool                      shadowsActive)
{
    m_ShadowMapSRV     = shadowMapSRV;
    m_WorldToLightProj = worldToLightProj;
    m_ShadowsActive    = shadowsActive && shadowMapSRV != nullptr && m_ShadowPSO && m_ShadowSRB;
}

void BasicForwardRenderer::Draw(Diligent::IDeviceContext* context,
                                entt::registry&           registry,
                                assets::AssetCache&       assetCache,
                                const Diligent::float4x4& viewProj,
                                bool                      skipGltfBacked)
{
    using namespace Diligent;

    if (context == nullptr || !m_PSO)
        return;

    const bool useShadows = m_ShadowsActive;
    IPipelineState*         pPSO = useShadows ? m_ShadowPSO.RawPtr() : m_PSO.RawPtr();
    IShaderResourceBinding* pSRB = useShadows ? m_ShadowSRB.RawPtr() : m_SRB.RawPtr();

    if (useShadows)
    {
        if (auto* var = pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_ShadowMap"))
            var->Set(m_ShadowMapSRV);
    }

    context->SetPipelineState(pPSO);
    context->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    auto view = registry.view<ecs::Transform, ecs::MeshRenderer>();
    for (auto entity : view)
    {
        if (registry.all_of<ecs::Visibility>(entity) && !registry.get<ecs::Visibility>(entity).InCamera)
            continue;

        const auto& transform = view.get<ecs::Transform>(entity);
        const auto& renderer  = view.get<ecs::MeshRenderer>(entity);

        const assets::AssetId meshId = ResolveCameraMeshId(registry, entity, renderer.MeshId);
        assets::MeshAssetPtr  mesh   = assetCache.GetOrLoad(meshId);
        if (mesh == nullptr || !mesh->VertexBuffer || !mesh->IndexBuffer || mesh->IndexCount == 0)
            continue;
        if (skipGltfBacked && mesh->GltfModel != nullptr)
            continue;

        {
            MapHelper<BasicVSConstants> cb(context, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            cb->WorldViewProj     = transform.ToMatrix() * viewProj;
            cb->World             = transform.ToMatrix();
            cb->WorldToLightProj  = m_WorldToLightProj;
            cb->ColorTint         = renderer.Color;
        }

        IBuffer*     pBuffs[] = {mesh->VertexBuffer};
        const Uint64 offset   = 0;
        context->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        context->SetIndexBuffer(mesh->IndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType  = mesh->IndexType;
        DrawAttrs.NumIndices = mesh->IndexCount;
        DrawAttrs.Flags      = DRAW_FLAG_VERIFY_ALL;
        context->DrawIndexed(DrawAttrs);
    }
}

} // namespace render
} // namespace sapana
