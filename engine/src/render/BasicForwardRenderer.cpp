#include "sapana/render/BasicForwardRenderer.hpp"

#include "MapHelper.hpp"
#include "sapana/ecs/Components.hpp"

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
    Diligent::float4   ColorTint;
};

} // namespace

bool BasicForwardRenderer::Initialize(Diligent::IRenderDevice*  device,
                                      Diligent::IEngineFactory* engineFactory,
                                      Diligent::ISwapChain*     swapChain,
                                      bool                      convertPSOutputToGamma)
{
    using namespace Diligent;

    if (device == nullptr || engineFactory == nullptr || swapChain == nullptr)
        return false;

    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name                                      = "Sapana Basic Forward PSO";
    PSOCreateInfo.PSODesc.PipelineType                              = PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets                 = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                    = swapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat                        = swapChain->GetDesc().DepthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology                = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode          = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable     = True;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderCI.CompileFlags                    = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    ShaderMacro Macros[] = {{"CONVERT_PS_OUTPUT_TO_GAMMA", convertPSOutputToGamma ? "1" : "0"}};
    ShaderCI.Macros      = {Macros, _countof(Macros)};

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    engineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Sapana Basic VS";
        ShaderCI.FilePath        = "cube.vsh";
        device->CreateShader(ShaderCI, &pVS);
        if (!pVS)
        {
            std::cerr << "Sapana BasicForwardRenderer: failed to create VS\n";
            return false;
        }

        BufferDesc CBDesc;
        CBDesc.Name           = "Sapana Basic VS CB";
        CBDesc.Size           = sizeof(BasicVSConstants);
        CBDesc.Usage          = USAGE_DYNAMIC;
        CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        device->CreateBuffer(CBDesc, nullptr, &m_VSConstants);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Sapana Basic PS";
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
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType  = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    device->CreateGraphicsPipelineState(PSOCreateInfo, &m_PSO);
    if (!m_PSO)
    {
        std::cerr << "Sapana BasicForwardRenderer: failed to create PSO\n";
        return false;
    }

    m_PSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    m_PSO->CreateShaderResourceBinding(&m_SRB, true);
    return true;
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

    context->SetPipelineState(m_PSO);
    context->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    auto view = registry.view<ecs::Transform, ecs::MeshRenderer>();
    for (auto entity : view)
    {
        const auto& transform = view.get<ecs::Transform>(entity);
        const auto& renderer  = view.get<ecs::MeshRenderer>(entity);

        assets::MeshAssetPtr mesh = assetCache.GetOrLoad(renderer.MeshId);
        if (mesh == nullptr || !mesh->VertexBuffer || !mesh->IndexBuffer || mesh->IndexCount == 0)
            continue;
        if (skipGltfBacked && mesh->GltfModel != nullptr)
            continue;

        {
            MapHelper<BasicVSConstants> cb(context, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            cb->WorldViewProj = transform.ToMatrix() * viewProj;
            cb->ColorTint     = renderer.Color;
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
