/*
 *  Copyright 2019-2025 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include "Tutorial02_Cube.hpp"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "ColorConversion.h"
#include "sapana/camera/CameraConfig.hpp"
#include "imgui.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial02_Cube();
}

void Tutorial02_Cube::CreatePipelineState()
{
    // Pipeline state object encompasses configuration of all GPU stages

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "Cube PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Cull back faces
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    // Enable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;

    // Pack matrices in row-major order
    ShaderCI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    // Presentation engine always expects input in gamma space. Normally, pixel shader output is
    // converted from linear to gamma space by the GPU. However, some platforms (e.g. Android in GLES mode,
    // or Emscripten in WebGL mode) do not support gamma-correction. In this case the application
    // has to do the conversion manually.
    ShaderMacro Macros[] = {{"CONVERT_PS_OUTPUT_TO_GAMMA", m_ConvertPSOutputToGamma ? "1" : "0"}};
    ShaderCI.Macros      = {Macros, _countof(Macros)};

    // In this tutorial, we will load shaders from file. To be able to do that,
    // we need to create a shader source stream factory
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube VS";
        ShaderCI.FilePath        = "cube.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
        // Create dynamic uniform buffer that will store our transformation matrix
        // Dynamic buffers can be frequently updated by the CPU
        BufferDesc CBDesc;
        CBDesc.Name           = "VS constants CB";
        CBDesc.Size           = sizeof(float4x4);
        CBDesc.Usage          = USAGE_DYNAMIC;
        CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_pDevice->CreateBuffer(CBDesc, nullptr, &m_VSConstants);
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube PS";
        ShaderCI.FilePath        = "cube.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    // clang-format off
    // Define vertex shader input layout
    LayoutElement LayoutElems[] =
    {
        // Attribute 0 - vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 1 - vertex color
        LayoutElement{1, 0, 4, VT_FLOAT32, False}
    };
    // clang-format on
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    // Define variable type that will be used by default
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);

    // Since we did not explicitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
    // change and are bound directly through the pipeline state object.
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

    // Create a shader resource binding object and bind all static resources in it
    m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);
}

void Tutorial02_Cube::CreateVertexBuffer()
{
    // Layout of this structure matches the one we defined in the pipeline state
    struct Vertex
    {
        float3 pos;
        float4 color;
    };

    // Cube vertices

    //      (-1,+1,+1)________________(+1,+1,+1)
    //               /|              /|
    //              / |             / |
    //             /  |            /  |
    //            /   |           /   |
    //(-1,-1,+1) /____|__________/(+1,-1,+1)
    //           |    |__________|____|
    //           |   /(-1,+1,-1) |    /(+1,+1,-1)
    //           |  /            |   /
    //           | /             |  /
    //           |/              | /
    //           /_______________|/
    //        (-1,-1,-1)       (+1,-1,-1)
    //

    constexpr Vertex CubeVerts[8] =
        {
            {float3{-1, -1, -1}, float4{1, 0, 0, 1}},
            {float3{-1, +1, -1}, float4{0, 1, 0, 1}},
            {float3{+1, +1, -1}, float4{0, 0, 1, 1}},
            {float3{+1, -1, -1}, float4{1, 1, 1, 1}},

            {float3{-1, -1, +1}, float4{1, 1, 0, 1}},
            {float3{-1, +1, +1}, float4{0, 1, 1, 1}},
            {float3{+1, +1, +1}, float4{1, 0, 1, 1}},
            {float3{+1, -1, +1}, float4{0.2f, 0.2f, 0.2f, 1.f}},
        };

    // Create a vertex buffer that stores cube vertices
    BufferDesc VertBuffDesc;
    VertBuffDesc.Name      = "Cube vertex buffer";
    VertBuffDesc.Usage     = USAGE_IMMUTABLE;
    VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    VertBuffDesc.Size      = sizeof(CubeVerts);
    BufferData VBData;
    VBData.pData    = CubeVerts;
    VBData.DataSize = sizeof(CubeVerts);
    m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_CubeVertexBuffer);
}

void Tutorial02_Cube::CreateIndexBuffer()
{
    // clang-format off
    constexpr Uint32 Indices[] =
    {
        2,0,1, 2,3,0,
        4,6,5, 4,7,6,
        0,7,4, 0,3,7,
        1,0,4, 1,4,5,
        1,5,2, 5,6,2,
        3,6,7, 3,2,6
    };
    // clang-format on

    BufferDesc IndBuffDesc;
    IndBuffDesc.Name      = "Cube index buffer";
    IndBuffDesc.Usage     = USAGE_IMMUTABLE;
    IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
    IndBuffDesc.Size      = sizeof(Indices);
    BufferData IBData;
    IBData.pData    = Indices;
    IBData.DataSize = sizeof(Indices);
    m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_CubeIndexBuffer);
}

void Tutorial02_Cube::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreatePipelineState();
    CreateVertexBuffer();
    CreateIndexBuffer();

    const bool isOpenGL = m_pDevice->GetDeviceInfo().NDC.MinZ == -1;

    sapana::camera::CameraConfig cameraConfig;
    if (!cameraConfig.LoadFromFile("config/camera.json"))
    {
        // Keep built-in defaults when config is missing (e.g. wrong CWD).
    }
    m_Camera.ApplyConfig(cameraConfig);
    m_Camera.SetPerspective(cameraConfig.FovYDegrees * (PI_F / 180.f), cameraConfig.NearPlane, cameraConfig.FarPlane, isOpenGL);

    if (!m_InputSystem.LoadBindings("config/input_bindings.json"))
    {
        // Defaults already installed by InputBindings.
    }

    m_CursorController.Initialize();
    m_CursorController.SetMode(sapana::input::CursorMode::Captured);
    // Avoid a one-frame look spike from wherever the OS cursor was on startup.
    m_InputSystem.SuppressLookFrames(1);
}

// Render a frame
void Tutorial02_Cube::Render()
{
    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = m_pSwapChain->GetDepthBufferDSV();
    // Clear the back buffer
    float4 ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};
    if (m_ConvertPSOutputToGamma)
    {
        // If manual gamma correction is required, we need to clear the render target with sRGB color
        ClearColor = LinearToSRGB(ClearColor);
    }
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        *CBConstants = m_WorldViewProjMatrix;
    }

    // Bind vertex and index buffers
    const Uint64 offset   = 0;
    IBuffer*     pBuffs[] = {m_CubeVertexBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs;     // This is an indexed draw call
    DrawAttrs.IndexType  = VT_UINT32; // Index type
    DrawAttrs.NumIndices = 36;
    // Verify the state of vertex and index buffers
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
}

void Tutorial02_Cube::Update(double CurrTime, double ElapsedTime, bool DoUpdateUI)
{
    SampleBase::Update(CurrTime, ElapsedTime, DoUpdateUI);
    (void)CurrTime;

    const auto& SCDesc  = m_pSwapChain->GetDesc();
    const int   centerX = static_cast<int>(SCDesc.Width / 2);
    const int   centerY = static_cast<int>(SCDesc.Height / 2);

    ImGuiIO* io = ImGui::GetCurrentContext() != nullptr ? &ImGui::GetIO() : nullptr;
    m_InputSystem.Update(GetInputController(), io);

    if (m_InputSystem.WasPressed(sapana::input::Action::ToggleCursor))
    {
        m_CursorController.Toggle();
        if (m_CursorController.IsCaptured())
        {
            m_CursorController.WarpPointer(centerX, centerY);
            m_InputSystem.NotifyPointerWarped();
            m_InputSystem.SuppressLookFrames(1);
        }
    }

    if (m_CursorController.IsCaptured())
    {
        float3 moveDir{0.f, 0.f, 0.f};
        if (m_InputSystem.IsDown(sapana::input::Action::MoveForward))
            moveDir.z += 1.f;
        if (m_InputSystem.IsDown(sapana::input::Action::MoveBackward))
            moveDir.z -= 1.f;
        if (m_InputSystem.IsDown(sapana::input::Action::MoveRight))
            moveDir.x += 1.f;
        if (m_InputSystem.IsDown(sapana::input::Action::MoveLeft))
            moveDir.x -= 1.f;
        if (m_InputSystem.IsDown(sapana::input::Action::MoveUp))
            moveDir.y += 1.f;
        if (m_InputSystem.IsDown(sapana::input::Action::MoveDown))
            moveDir.y -= 1.f;
        m_Camera.Move(moveDir, static_cast<float>(ElapsedTime));

        const auto look = m_InputSystem.GetLookDelta();
        m_Camera.LookPixels(look.x, look.y);

        // Keep pointer near center for infinite free-look. Look itself stays
        // frame-to-frame; the next Update absorbs the warp landing (no drift).
        if (look.x != 0.f || look.y != 0.f)
        {
            m_CursorController.WarpPointer(centerX, centerY);
            m_InputSystem.NotifyPointerWarped();
        }
    }

    // Static cube; view comes from the camera
    const float4x4 CubeModelTransform = float4x4::Identity();
    const float4x4 View               = m_Camera.GetViewMatrix();
    const float4x4 SrfPreTransform    = GetSurfacePretransformMatrix(float3{0, 0, 1});

    const float    aspect = static_cast<float>(SCDesc.Width) / static_cast<float>(SCDesc.Height);
    const float4x4 Proj   = m_Camera.GetProjectionMatrix(aspect);

    m_WorldViewProjMatrix = CubeModelTransform * View * SrfPreTransform * Proj;
}

} // namespace Diligent
