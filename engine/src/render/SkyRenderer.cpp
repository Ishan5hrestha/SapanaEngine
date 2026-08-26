#include "sapana/render/SkyRenderer.hpp"

#include "MapHelper.hpp"

#include <cstring>
#include <iostream>

namespace sapana
{
namespace render
{

namespace
{

struct SkyConstants
{
    Diligent::float4x4 InvViewProj;
    Diligent::float4   Zenith;
    Diligent::float4   Horizon;
    Diligent::float4   Ground;
    Diligent::float4   FalloffPad; // x = falloff (std140-aligned)
};

} // namespace

bool SkyRenderer::Initialize(Diligent::IRenderDevice*  device,
                             Diligent::IEngineFactory* engineFactory,
                             Diligent::ISwapChain*     swapChain,
                             bool                      convertPSOutputToGamma)
{
    using namespace Diligent;

    if (device == nullptr || engineFactory == nullptr || swapChain == nullptr)
        return false;

    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name                         = "Sapana Sky PSO";
    PSOCreateInfo.PSODesc.PipelineType                 = PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets    = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]       = swapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat           = swapChain->GetDesc().DepthBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology   = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = False;

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
        ShaderCI.Desc.Name       = "Sapana Sky VS";
        ShaderCI.FilePath        = "shaders/sky.vsh";
        device->CreateShader(ShaderCI, &pVS);
        if (!pVS)
        {
            std::cerr << "Sapana SkyRenderer: failed to create VS (shaders/sky.vsh)\n";
            return false;
        }
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Sapana Sky PS";
        ShaderCI.FilePath        = "shaders/sky.psh";
        device->CreateShader(ShaderCI, &pPS);
        if (!pPS)
        {
            std::cerr << "Sapana SkyRenderer: failed to create PS (shaders/sky.psh)\n";
            return false;
        }
    }

    BufferDesc CBDesc;
    CBDesc.Name           = "Sapana Sky CB";
    CBDesc.Size           = sizeof(SkyConstants);
    CBDesc.Usage          = USAGE_DYNAMIC;
    CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
    CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    device->CreateBuffer(CBDesc, nullptr, &m_Constants);
    if (!m_Constants)
    {
        std::cerr << "Sapana SkyRenderer: failed to create constant buffer\n";
        return false;
    }

    // VertexID-only; no vertex buffers / input layout.
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = nullptr;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = 0;
    PSOCreateInfo.pVS                                         = pVS;
    PSOCreateInfo.pPS                                         = pPS;
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType  = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    device->CreateGraphicsPipelineState(PSOCreateInfo, &m_PSO);
    if (!m_PSO)
    {
        std::cerr << "Sapana SkyRenderer: failed to create PSO\n";
        return false;
    }

    // Same CB bound to VS + PS (Diligent static vars).
    if (auto* pVSVar = m_PSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants"))
        pVSVar->Set(m_Constants);
    if (auto* pPSVar = m_PSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "Constants"))
        pPSVar->Set(m_Constants);

    m_PSO->CreateShaderResourceBinding(&m_SRB, true);
    if (!m_SRB)
    {
        std::cerr << "Sapana SkyRenderer: failed to create SRB\n";
        return false;
    }

    return true;
}

void SkyRenderer::ApplyConfig(const SkyConfig& config)
{
    m_Config = config;
    if (m_Config.HorizonFalloff < 0.01f)
        m_Config.HorizonFalloff = 0.01f;
}

void SkyRenderer::Draw(Diligent::IDeviceContext* context,
                       const Diligent::float4x4& view,
                       const Diligent::float4x4& proj)
{
    using namespace Diligent;

    if (context == nullptr || !IsReady() || !m_Config.Enabled)
        return;

    // Strip camera translation so the sky is infinitely far.
    float4x4 viewRot = view;
    viewRot._41      = 0.f;
    viewRot._42      = 0.f;
    viewRot._43      = 0.f;

    const float4x4 viewProj    = viewRot * proj;
    const float4x4 invViewProj = viewProj.Inverse();

    {
        MapHelper<SkyConstants> cb{context, m_Constants, MAP_WRITE, MAP_FLAG_DISCARD};
        cb->InvViewProj = invViewProj;
        cb->Zenith      = float4{m_Config.ZenithColor, 1.f};
        cb->Horizon     = float4{m_Config.HorizonColor, 1.f};
        cb->Ground      = float4{m_Config.GroundColor, 1.f};
        cb->FalloffPad  = float4{m_Config.HorizonFalloff, 0.f, 0.f, 0.f};
    }

    context->SetPipelineState(m_PSO);
    context->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 3;
    drawAttrs.Flags       = DRAW_FLAG_VERIFY_ALL;
    context->Draw(drawAttrs);
}

} // namespace render
} // namespace sapana
