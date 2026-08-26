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

constexpr float kChaseBack = 6.f;
constexpr float kChaseUp   = 2.5f;

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

/// World velocity matching Camera::Move (yaw-relative horiz, world-up vertical).
float3 MakeFlyVelocity(const float3& moveDir, float yawRadians, float moveSpeed)
{
    float3 dir = moveDir;
    const float len = length(dir);
    if (len == 0.f)
        return float3{0.f, 0.f, 0.f};
    dir /= len;

    const float3 localHoriz{dir.x, 0.f, dir.z};
    const float3 worldHoriz = localHoriz * float4x4::RotationY(yawRadians).Transpose();
    const float3 worldDir{worldHoriz.x, dir.y, worldHoriz.z};
    return worldDir * moveSpeed;
}

/// Third-person view: behind and above the drone, same yaw/pitch as freelook Camera.
float4x4 MakeChaseViewMatrix(const float3& target, float yaw, float pitch)
{
    const float3 eye =
        target + float3{-std::sin(yaw) * kChaseBack, kChaseUp, -std::cos(yaw) * kChaseBack};
    return float4x4::Translation(-eye) * float4x4::RotationY(yaw) * float4x4::RotationX(pitch);
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
        m_DronePitch  = t.RotationDegrees.x * (PI_F / 180.f);
        m_DronePitch  = std::clamp(m_DronePitch, -m_PitchLimit, m_PitchLimit);
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

    if (!sapana::scene::SceneLoader::Load("scenes/sandbox.json", m_Registry, m_AssetCache))
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
        if (m_DroneEntity != entt::null && m_Registry.all_of<sapana::physics::PhysicsBody>(m_DroneEntity))
        {
            const auto id = m_Registry.get<sapana::physics::PhysicsBody>(m_DroneEntity).Id;
            m_Physics.SetGravityFactor(id, 0.f);
        }
    }

    const bool isOpenGL = m_pDevice->GetDeviceInfo().NDC.MinZ == -1;

    sapana::camera::CameraConfig cameraConfig;
    if (!cameraConfig.LoadFromFile("config/camera.json"))
    {
        // Keep built-in defaults when config is missing (e.g. wrong CWD).
    }
    m_Camera.ApplyConfig(cameraConfig);
    m_Camera.SetPerspective(cameraConfig.FovYDegrees * (PI_F / 180.f), cameraConfig.NearPlane, cameraConfig.FarPlane, isOpenGL);
    m_DroneMoveSpeed  = cameraConfig.MoveSpeed;
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
    float4        ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};
    if (m_ConvertPSOutputToGamma)
    {
        ClearColor = LinearToSRGB(ClearColor);
    }
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

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

        const auto look = m_InputSystem.GetLookDelta();

        if (m_ControlMode == ControlMode::Freelook)
        {
            m_Camera.Move(moveDir, dt);
            m_Camera.LookPixels(look.x, look.y);
        }
        else if (m_DroneEntity != entt::null && m_Registry.valid(m_DroneEntity) &&
                 m_Registry.all_of<sapana::physics::PhysicsBody>(m_DroneEntity))
        {
            m_DroneYaw += -look.x * m_LookSensitivity;
            m_DronePitch += -look.y * m_LookSensitivity;
            m_DronePitch = std::clamp(m_DronePitch, -m_PitchLimit, m_PitchLimit);

            const auto bodyId = m_Registry.get<sapana::physics::PhysicsBody>(m_DroneEntity).Id;
            m_Physics.SetLinearVelocity(bodyId, MakeFlyVelocity(moveDir, m_DroneYaw, m_DroneMoveSpeed));
            m_Physics.SetRotationDegrees(
                bodyId,
                float3{m_DronePitch * (180.f / PI_F), m_DroneYaw * (180.f / PI_F), 0.f});
        }

        if (look.x != 0.f || look.y != 0.f)
        {
            m_CursorController.WarpPointer(centerX, centerY);
            m_InputSystem.NotifyPointerWarped();
        }
    }
    else if (m_ControlMode == ControlMode::Drone && m_DroneEntity != entt::null &&
             m_Registry.valid(m_DroneEntity) &&
             m_Registry.all_of<sapana::physics::PhysicsBody>(m_DroneEntity))
    {
        // Cursor free: stop drone so it does not keep drifting.
        const auto bodyId = m_Registry.get<sapana::physics::PhysicsBody>(m_DroneEntity).Id;
        m_Physics.SetLinearVelocity(bodyId, float3{0.f, 0.f, 0.f});
    }

    if (m_Physics.IsInitialized())
        m_Physics.Update(m_Registry, dt);

    float4x4 View;
    if (m_ControlMode == ControlMode::Drone && m_DroneEntity != entt::null &&
        m_Registry.valid(m_DroneEntity) && m_Registry.all_of<sapana::ecs::Transform>(m_DroneEntity))
    {
        const auto& t = m_Registry.get<sapana::ecs::Transform>(m_DroneEntity);
        View          = MakeChaseViewMatrix(t.Position, m_DroneYaw, m_DronePitch);
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
}

} // namespace Diligent
