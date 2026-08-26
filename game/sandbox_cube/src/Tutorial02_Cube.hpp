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

#pragma once

#include "SampleBase.hpp"
#include "BasicMath.hpp"
#include "sapana/assets/AssetCache.hpp"
#include "sapana/camera/Camera.hpp"
#include "sapana/flight/FlightConfig.hpp"
#include "sapana/flight/FlightController.hpp"
#include "sapana/flight/QuadDynamics.hpp"
#include "sapana/input/CursorController.hpp"
#include "sapana/input/InputSystem.hpp"
#include "sapana/render/BasicForwardRenderer.hpp"
#include "sapana/render/PbrGltfRenderer.hpp"
#include "sapana/render/LightingConfig.hpp"
#include "sapana/render/LodConfig.hpp"
#include "sapana/render/SkyRenderer.hpp"
#include "sapana/render/ShadowSystem.hpp"
#include "sapana/render/RenderMode.hpp"
#include "sapana/render/VisibilityAndLodSystem.hpp"
#include "sapana/physics/PhysicsSystem.hpp"

#include <entt/entt.hpp>
#include <memory>

namespace Diligent
{

enum class CameraMode
{
    Freelook,
    Chase,
    FpvNose
};

class Tutorial02_Cube final : public SampleBase
{
public:
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime, bool DoUpdateUI) override final;

    virtual const Char* GetSampleName() const override final { return "Sapana Sandbox"; }

private:
    void FindDroneEntity();
    void CycleCameraMode();
    void PrintModeLine() const;
    bool IsDroneCamera() const;

    sapana::camera::Camera                 m_Camera;
    sapana::input::InputSystem             m_InputSystem;
    sapana::input::CursorController        m_CursorController;
    sapana::assets::AssetCache             m_AssetCache;
    sapana::render::BasicForwardRenderer   m_BasicRenderer;
    sapana::render::PbrGltfRenderer        m_PbrRenderer;
    sapana::render::SkyRenderer            m_SkyRenderer;
    sapana::render::ShadowSystem           m_ShadowSystem;
    sapana::render::LightingConfig           m_LightingConfig;
    sapana::render::LodConfig                m_LodConfig;
    sapana::render::VisibilityAndLodSystem   m_VisibilityLod;
    sapana::physics::PhysicsSystem           m_Physics;
    sapana::flight::FlightConfig             m_FlightConfig;
    sapana::flight::FlightController         m_FlightController;
    sapana::flight::QuadDynamics             m_QuadDynamics;
    sapana::render::RenderMode             m_RenderMode = sapana::render::RenderMode::Basic;
    entt::registry                         m_Registry;
    float4x4                               m_ViewMatrix;
    float4x4                               m_ProjMatrix;
    float4x4                               m_ViewProjMatrix;

    CameraMode    m_CameraMode   = CameraMode::Freelook;
    entt::entity  m_DroneEntity  = entt::null;
    float         m_LookSensitivity = 0.003f;
    float         m_PitchLimit   = PI_F / 2.f - 0.01f;
};

} // namespace Diligent
