#pragma once

#include "sapana/physics/PhysicsSystem.hpp"

#include <entt/entt.hpp>
#include <unordered_map>

namespace sapana
{
namespace physics
{

/// Applies ForceMotor + MotorInput as forces before PhysicsSystem::Update.
/// Reusable for drones, vehicles, etc.
class ForceMotorSystem
{
public:
    /// Sets this frame's drive input for an entity that has ForceMotor + PhysicsBody.
    void SetInput(entt::entity              entity,
                  const Diligent::float3&   localMove,
                  float                     yawRadians,
                  bool                      thrust,
                  bool                      active);

    /// Clears input so the motor stops driving (drag may still apply if body moving).
    void ClearInput(entt::entity entity);

    /// Apply drive forces + drag. Call once per frame before PhysicsSystem::Update.
    void Update(entt::registry& registry, PhysicsSystem& physics);

    /// Clamp |v| to MaxSpeed. Call once per frame after PhysicsSystem::Update.
    void ClampSpeeds(entt::registry& registry, PhysicsSystem& physics);

private:
    struct FrameInput
    {
        Diligent::float3 LocalMove{0.f, 0.f, 0.f};
        float            YawRadians = 0.f;
        bool             Thrust     = false;
        bool             Active     = false;
    };

    std::unordered_map<entt::entity, FrameInput> m_Inputs;
};

} // namespace physics
} // namespace sapana
