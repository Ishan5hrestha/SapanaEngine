#include "sapana/physics/ForceMotorSystem.hpp"

#include "sapana/physics/PhysicsComponents.hpp"

#include <cmath>

namespace sapana
{
namespace physics
{

namespace
{

using Diligent::float3;
using Diligent::float4x4;
using Diligent::length;

float3 YawRelativeHorizontal(const float3& localMove, float yawRadians)
{
    float3 dir = localMove;
    dir.y      = 0.f;
    const float len = length(dir);
    if (len < 1e-5f)
        return float3{0.f, 0.f, 0.f};
    dir /= len;
    return dir * float4x4::RotationY(yawRadians).Transpose();
}

} // namespace

void ForceMotorSystem::SetInput(entt::entity            entity,
                                const Diligent::float3& localMove,
                                float                   yawRadians,
                                bool                    thrust,
                                bool                    active)
{
    FrameInput& in = m_Inputs[entity];
    in.LocalMove   = localMove;
    in.YawRadians  = yawRadians;
    in.Thrust      = thrust;
    in.Active      = active;
}

void ForceMotorSystem::ClearInput(entt::entity entity)
{
    m_Inputs.erase(entity);
}

void ForceMotorSystem::Update(entt::registry& registry, PhysicsSystem& physics)
{
    if (!physics.IsInitialized())
        return;

    auto view = registry.view<ForceMotor, PhysicsBody>();
    for (auto entity : view)
    {
        const auto& motor = view.get<ForceMotor>(entity);
        if (!motor.Enabled)
            continue;

        const BodyId id = view.get<PhysicsBody>(entity).Id;
        if (id == kInvalidBodyId)
            continue;

        FrameInput in{};
        const auto it = m_Inputs.find(entity);
        if (it != m_Inputs.end())
            in = it->second;

        float3 force{0.f, 0.f, 0.f};

        if (in.Active)
        {
            const float3 horiz = YawRelativeHorizontal(in.LocalMove, in.YawRadians);
            force += horiz * motor.HorizontalForce;

            const bool lift = in.Thrust || in.LocalMove.y > 0.f;
            if (lift)
                force.y += motor.ThrustForce;
            else if (in.LocalMove.y < 0.f)
                force.y -= motor.DownForce;
        }

        const float3 vel = physics.GetLinearVelocity(id);
        force += float3{-vel.x * motor.Drag, -vel.y * motor.Drag * 0.35f, -vel.z * motor.Drag};

        if (std::fabs(force.x) > 1e-6f || std::fabs(force.y) > 1e-6f || std::fabs(force.z) > 1e-6f)
            physics.AddForce(id, force);
    }

    // Inputs are one-shot per frame; caller must SetInput again next frame.
    m_Inputs.clear();
}

void ForceMotorSystem::ClampSpeeds(entt::registry& registry, PhysicsSystem& physics)
{
    if (!physics.IsInitialized())
        return;

    auto view = registry.view<ForceMotor, PhysicsBody>();
    for (auto entity : view)
    {
        const auto& motor = view.get<ForceMotor>(entity);
        if (!motor.Enabled)
            continue;

        const BodyId id = view.get<PhysicsBody>(entity).Id;
        if (id == kInvalidBodyId)
            continue;

        const float3 vel   = physics.GetLinearVelocity(id);
        const float  speed = length(vel);
        if (speed > motor.MaxSpeed && speed > 1e-5f)
        {
            const float3 clamped = (vel / speed) * motor.MaxSpeed;
            physics.SetLinearVelocity(id, clamped);
        }
    }
}

} // namespace physics
} // namespace sapana
