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
 *  In no event and under any legal theory, whether in tort (including negligence),
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

#include "ColorConversion.h"
#include "sapana/camera/CameraConfig.hpp"
#include "sapana/ecs/Components.hpp"
#include "sapana/physics/PhysicsComponents.hpp"
#include "sapana/physics/PhysicsConfig.hpp"
#include "sapana/render/LightingConfig.hpp"
#include "sapana/scene/SceneLoader.hpp"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

namespace Diligent
{

namespace
{

constexpr float kAttachBack  = 3.5f;  // meters behind drone (local -Z)
constexpr float kAttachUp    = 1.2f;  // meters above drone
constexpr float kAttachPitch = -0.28f; // slight look-down (matches freelook Camera pitch sign)
constexpr float kDroneYawRate = 2.2f; // rad/s for A/D turn

sapana::render::RenderMode LoadRenderMode(const char* path)
{
    std::ifstream file(path);
    if (!file)
        return sapana::render::RenderMode::Basic;

    try
    {
        nlohmann::json root;
        file >> root;
        if (root.contains("mode") && root.at("mode").is_string())
        {
            const std::string mode = root.at("mode").get<std::string>();
            if (mode == "pbr" || mode == "PBR")
                return sapana::render::RenderMode::PBR;
        }
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "Sapana: failed to parse renderer config: " << ex.what() << '\n';
    }
    return sapana::render::RenderMode::Basic;
}

/// Camera rigidly parented to the drone: fixed local offset, same yaw/facing as the craft.
/// No independent look-at — when the drone turns, the camera turns with it.
float4x4 MakeDroneAttachedView(const float3& dronePos, float yaw)
{
    // Local (+X right, +Y up, +Z forward) → world, same basis as ForceMotor.
    const float3 localOffset{0.f, kAttachUp, -kAttachBack};
    const float3 worldOffset = localOffset * float4x4::RotationY(yaw).Transpose();
    const float3 eye         = dronePos + worldOffset;

    // Same construction as sapana::camera::Camera::UpdateViewMatrix.
    const float4x4 cameraRotation = float4x4::RotationY(yaw) * float4x4::RotationX(kAttachPitch);
    return float4x4::Translation(-eye) * cameraRotation;
}

} // namespace

SampleBase* CreateSample()
{
    return new Tutorial02_Cube();
}

void Tutorial02_Cube::FindDroneEntity()
{
    m_DroneEntity = entt::null;
    auto view     = m_Registry.view<sapana::ecs::Name>();
    for (auto entity : view)
    {
        if (view.get<sapana::ecs::Name>(entity).Value == "Drone")
        {
            m_DroneEntity = entity;
            return;
        }
    }
}

void Tutorial02_Cube::EnterDroneMode()
{
    if (m_DroneEntity == entt::null || !m_Registry.valid(m_DroneEntity))
    {
        std::cerr << "Sapana: cannot enter drone mode (Drone entity missing)\n";
        return;
    }

    m_ControlMode = ControlMode::Drone;
    if (m_Registry.all_of<sapana::ecs::Transform>(m_DroneEntity))
    {
        const auto& t = m_Registry.get<sapana::ecs::Transform>(m_DroneEntity);
        m_DroneYaw    = t.RotationDegrees.y * (PI_F / 180.f);
    }
    m_InputSystem.SuppressLookFrames(1);
    std::cerr << "Sapana: control mode = Drone (chase cam). Press K for freelook.\n";
}

void Tutorial02_Cube::EnterFreelookMode()
{
    m_ControlMode = ControlMode::Freelook;
    if (m_DroneEntity != entt::null && m_Registry.valid(m_DroneEntity) &&
        m_Registry.all_of<sapana::physics::PhysicsBody>(m_DroneEntity))
    {
        // Stop residual velocity when leaving drone control.
        const auto id = m_Registry.get<sapana::physics::PhysicsBody>(m_DroneEntity).Id;
        m_Physics.SetLinearVelocity(id, float3{0.f, 0.f, 0.f});
    }
    m_InputSystem.SuppressLookFrames(1);
    std::cerr << "Sapana: control mode = Freelook. Press K for drone.\n";
}

void Tutorial02_Cube::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    m_AssetCache.SetDevice(m_pDevice, m_pImmediateContext);
    if (!m_BasicRenderer.Initialize(m_pDevice, m_pEngineFactory, m_pSwapChain, m_ConvertPSOutputToGamma))
    {
        std::cerr << "Sapana: BasicForwardRenderer failed to initialize\n";
    }

    m_RenderMode = LoadRenderMode("config/renderer.json");

    if (!m_LightingConfig.LoadFromFile("config/lighting.json"))
    {
        std::cerr << "Sapana: using default lighting config (config/lighting.json missing or invalid)\n";
    }
    m_PbrRenderer.ApplyConfig(m_LightingConfig);
    m_SkyRenderer.ApplyConfig(m_LightingConfig.Sky);
    m_ShadowSystem.ApplyConfig(m_LightingConfig.Shadows);

    if (!m_SkyRenderer.Initialize(m_pDevice, m_pEngineFactory, m_pSwapChain, m_ConvertPSOutputToGamma))
    {
        std::cerr << "Sapana: SkyRenderer failed to initialize (gradient sky disabled)\n";
    }

    if (!m_ShadowSystem.Initialize(m_pDevice, m_pEngineFactory, m_pSwapChain))
    {
        std::cerr << "Sapana: ShadowSystem failed to initialize (shadows disabled)\n";
    }
    else if (m_ShadowSystem.IsEnabled())
    {
        std::cerr << "Sapana: shadows enabled (PCF)\n";
    }

    if (m_RenderMode == sapana::render::RenderMode::PBR)
    {
        if (!m_PbrRenderer.Initialize(m_pDevice, m_pImmediateContext, m_pSwapChain, m_ConvertPSOutputToGamma))
        {
            std::cerr << "Sapana: PBR mode requested but PbrGltfRenderer failed; falling back to Basic\n";
            m_RenderMode = sapana::render::RenderMode::Basic;
        }
        else
        {
            std::cerr << "Sapana: render mode = PBR\n";
        }
    }
    else
    {
        std::cerr << "Sapana: render mode = Basic\n";
    }

    if (!m_LodConfig.LoadFromFile("config/lod.json"))
    {
        std::cerr << "Sapana: using default LOD config (config/lod.json missing or invalid)\n";
    }
    m_VisibilityLod.ApplyConfig(m_LodConfig);

    if (!sapana::scene::SceneLoader::Load("scenes/sandbox.json", m_Registry, m_AssetCache, &m_LodConfig))
    {
        std::cerr << "Sapana: failed to load scenes/sandbox.json\n";
    }

    FindDroneEntity();
    if (m_DroneEntity == entt::null)
        std::cerr << "Sapana: scene has no entity named Drone; K toggle disabled\n";

    sapana::physics::PhysicsConfig physicsConfig;
    if (!physicsConfig.LoadFromFile("config/physics.json"))
    {
        std::cerr << "Sapana: using default physics config (config/physics.json missing or invalid)\n";
    }
    if (!m_Physics.Initialize(physicsConfig))
    {
        std::cerr << "Sapana: PhysicsSystem failed to initialize\n";
    }
    else
    {
        m_Physics.CreateBodies(m_Registry);
    }

    const bool isOpenGL = m_pDevice->GetDeviceInfo().NDC.MinZ == -1;

    sapana::camera::CameraConfig cameraConfig;
    if (!cameraConfig.LoadFromFile("config/camera.json"))
    {
        // Keep built-in defaults when config is missing (e.g. wrong CWD).
    }
    m_Camera.ApplyConfig(cameraConfig);

    // Keep GPU far clip beyond CPU far-cull so meshes are removed whole, not sliced by the far plane.
    if (m_LodConfig.Enabled)
    {
        constexpr float kFarPlaneMargin = 50.f;
        const float     farNeeded =
            m_LodConfig.FarCullDistance + m_LodConfig.Hysteresis + kFarPlaneMargin;
        if (cameraConfig.FarPlane < farNeeded)
        {
            std::cerr << "Sapana: raising camera far_plane from " << cameraConfig.FarPlane << " to " << farNeeded
                      << " (margin past far_cull_distance)\n";
            cameraConfig.FarPlane = farNeeded;
        }
    }

    m_Camera.SetPerspective(cameraConfig.FovYDegrees * (PI_F / 180.f), cameraConfig.NearPlane, cameraConfig.FarPlane, isOpenGL);
    m_LookSensitivity = cameraConfig.LookSensitivity;
    m_PitchLimit      = cameraConfig.PitchLimitDegrees * (PI_F / 180.f);

    if (!m_InputSystem.LoadBindings("config/input_bindings.json"))
    {
        // Defaults already installed by InputBindings.
    }

    m_CursorController.Initialize();
    m_CursorController.SetMode(sapana::input::CursorMode::Captured);
    m_InputSystem.SuppressLookFrames(1);
    std::cerr << "Sapana: control mode = Freelook. Press K for drone chase.\n";
}

void Tutorial02_Cube::Render()
{
    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = m_pSwapChain->GetDepthBufferDSV();

    // Shadow pass (offscreen) before main color targets.
    if (m_ShadowSystem.IsEnabled())
    {
        const float3 lightDir = m_LightingConfig.Lights.empty()
                                    ? float3{-0.4f, -1.f, -0.3f}
                                    : m_LightingConfig.Lights.front().Direction;
        m_ShadowSystem.Update(m_ViewMatrix, m_ProjMatrix, lightDir, 0.1f,
                              m_LightingConfig.Shadows.MaxDistance);
        m_ShadowSystem.RenderCasters(m_pImmediateContext, m_Registry, m_AssetCache);
        m_PbrRenderer.SetShadowResources(m_ShadowSystem.GetShadowMapSRV(),
                                         m_ShadowSystem.GetWorldToLightProj(), true);
        m_BasicRenderer.SetShadowResources(m_ShadowSystem.GetShadowMapSRV(),
                                           m_ShadowSystem.GetWorldToLightProj(), true);
    }
    else
    {
        m_PbrRenderer.SetShadowResources(nullptr, float4x4::Identity(), false);
        m_BasicRenderer.SetShadowResources(nullptr, float4x4::Identity(), false);
    }

    // Restore main render targets after shadow cascades.
    m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    float4 ClearColor = m_LightingConfig.ClearColor;
    if (m_ConvertPSOutputToGamma)
    {
        ClearColor = LinearToSRGB(ClearColor);
    }
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (m_SkyRenderer.IsReady() && m_SkyRenderer.IsEnabled())
        m_SkyRenderer.Draw(m_pImmediateContext, m_ViewMatrix, m_ProjMatrix);

    if (m_RenderMode == sapana::render::RenderMode::PBR && m_PbrRenderer.IsReady())
    {
        m_BasicRenderer.Draw(m_pImmediateContext, m_Registry, m_AssetCache, m_ViewProjMatrix, true);
        const auto& SCDesc = m_pSwapChain->GetDesc();
        m_PbrRenderer.Draw(m_pImmediateContext, m_Registry, m_AssetCache,
                           m_ViewMatrix, m_ProjMatrix, m_ViewProjMatrix,
                           SCDesc.Width, SCDesc.Height);
    }
    else
    {
        m_BasicRenderer.Draw(m_pImmediateContext, m_Registry, m_AssetCache, m_ViewProjMatrix);
    }
}

void Tutorial02_Cube::Update(double CurrTime, double ElapsedTime, bool DoUpdateUI)
{
    SampleBase::Update(CurrTime, ElapsedTime, DoUpdateUI);
    (void)CurrTime;

    const auto& SCDesc  = m_pSwapChain->GetDesc();
    const int   centerX = static_cast<int>(SCDesc.Width / 2);
    const int   centerY = static_cast<int>(SCDesc.Height / 2);
    const float dt      = static_cast<float>(ElapsedTime);

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

    if (m_InputSystem.WasPressed(sapana::input::Action::ToggleControlMode))
    {
        if (m_ControlMode == ControlMode::Freelook)
            EnterDroneMode();
        else
            EnterFreelookMode();
    }

    if (m_CursorController.IsCaptured())
    {
        const auto look = m_InputSystem.GetLookDelta();

        if (m_ControlMode == ControlMode::Freelook)
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

            m_Camera.Move(moveDir, dt);
            m_Camera.LookPixels(look.x, look.y);
        }
        else if (m_DroneEntity != entt::null && m_Registry.valid(m_DroneEntity) &&
                 m_Registry.all_of<sapana::physics::PhysicsBody>(m_DroneEntity))
        {
            // Tank-style: A/D yaw the drone body, W/S thrust along facing; Space lift only.
            // Mouse look is ignored in drone mode.
            if (m_InputSystem.IsDown(sapana::input::Action::MoveLeft))
                m_DroneYaw += kDroneYawRate * dt;
            if (m_InputSystem.IsDown(sapana::input::Action::MoveRight))
                m_DroneYaw -= kDroneYawRate * dt;

            float3 localMove{0.f, 0.f, 0.f};
            if (m_InputSystem.IsDown(sapana::input::Action::MoveForward))
                localMove.z += 1.f;
            if (m_InputSystem.IsDown(sapana::input::Action::MoveBackward))
                localMove.z -= 1.f;

            const float yawDegrees = m_DroneYaw * (180.f / PI_F);
            const auto  bodyId     = m_Registry.get<sapana::physics::PhysicsBody>(m_DroneEntity).Id;
            m_Physics.SetRotationDegrees(bodyId, float3{0.f, yawDegrees, 0.f});
            if (m_Registry.all_of<sapana::ecs::Transform>(m_DroneEntity))
                m_Registry.get<sapana::ecs::Transform>(m_DroneEntity).RotationDegrees =
                    float3{0.f, yawDegrees, 0.f};

            const bool thrust = m_InputSystem.IsDown(sapana::input::Action::Thrust);
            m_ForceMotors.SetInput(m_DroneEntity, localMove, m_DroneYaw, thrust, true);
        }

        if (look.x != 0.f || look.y != 0.f)
        {
            m_CursorController.WarpPointer(centerX, centerY);
            m_InputSystem.NotifyPointerWarped();
        }
    }
    else if (m_ControlMode == ControlMode::Drone && m_DroneEntity != entt::null)
    {
        // Cursor free: no motor drive; gravity + drag still apply.
        m_ForceMotors.ClearInput(m_DroneEntity);
    }

    if (m_Physics.IsInitialized())
    {
        m_ForceMotors.Update(m_Registry, m_Physics);
        m_Physics.Update(m_Registry, dt);
        m_ForceMotors.ClampSpeeds(m_Registry, m_Physics);
    }

    // Keep authored yaw on the drone after physics sync (euler extraction can drift).
    if (m_ControlMode == ControlMode::Drone && m_DroneEntity != entt::null &&
        m_Registry.valid(m_DroneEntity) &&
        m_Registry.all_of<sapana::ecs::Transform, sapana::physics::PhysicsBody>(m_DroneEntity))
    {
        const float yawDegrees = m_DroneYaw * (180.f / PI_F);
        m_Registry.get<sapana::ecs::Transform>(m_DroneEntity).RotationDegrees =
            float3{0.f, yawDegrees, 0.f};
        m_Physics.SetRotationDegrees(m_Registry.get<sapana::physics::PhysicsBody>(m_DroneEntity).Id,
                                     float3{0.f, yawDegrees, 0.f});
    }

    float4x4 View;
    if (m_ControlMode == ControlMode::Drone && m_DroneEntity != entt::null &&
        m_Registry.valid(m_DroneEntity) && m_Registry.all_of<sapana::ecs::Transform>(m_DroneEntity))
    {
        const auto& t = m_Registry.get<sapana::ecs::Transform>(m_DroneEntity);
        View          = MakeDroneAttachedView(t.Position, m_DroneYaw);
    }
    else
    {
        View = m_Camera.GetViewMatrix();
    }

    const float4x4 SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});
    const float    aspect          = static_cast<float>(SCDesc.Width) / static_cast<float>(SCDesc.Height);
    const float4x4 Proj            = m_Camera.GetProjectionMatrix(aspect);
    m_ViewMatrix                   = View * SrfPreTransform;
    m_ProjMatrix                   = Proj;
    m_ViewProjMatrix               = m_ViewMatrix * m_ProjMatrix;

    float3 cameraPos{0.f, 0.f, 0.f};
    if (m_ControlMode == ControlMode::Drone && m_DroneEntity != entt::null &&
        m_Registry.valid(m_DroneEntity) && m_Registry.all_of<sapana::ecs::Transform>(m_DroneEntity))
    {
        // Approximate eye from drone-attached view (same offset as MakeDroneAttachedView).
        const auto& t = m_Registry.get<sapana::ecs::Transform>(m_DroneEntity);
        const float3 localOffset{0.f, kAttachUp, -kAttachBack};
        const float3 worldOffset = localOffset * float4x4::RotationY(m_DroneYaw).Transpose();
        cameraPos                = t.Position + worldOffset;
    }
    else
    {
        cameraPos = m_Camera.GetPosition();
    }

    const bool isGL = m_pDevice->GetDeviceInfo().IsGLDevice();
    m_VisibilityLod.Update(m_Registry, cameraPos, m_ViewProjMatrix, isGL);
}

} // namespace Diligent
