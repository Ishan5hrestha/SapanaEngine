#pragma once

#include "BasicMath.hpp"
#include "sapana/physics/PhysicsConfig.hpp"
#include "sapana/physics/PhysicsTypes.hpp"

#include <entt/entt.hpp>
#include <memory>

namespace sapana
{
namespace physics
{

/// Owns the Jolt world. Public API never exposes Jolt types.
///
/// Usage:
///   1. Initialize(config)
///   2. CreateBodies(registry) after scene load (entities with RigidBody+Collider)
///   3. Update(registry, dt) each frame — fixed step, syncs dynamic Transforms
///   4. Shutdown() on teardown
class PhysicsSystem
{
public:
    PhysicsSystem();
    ~PhysicsSystem();

    PhysicsSystem(const PhysicsSystem&)            = delete;
    PhysicsSystem& operator=(const PhysicsSystem&) = delete;

    bool Initialize(const PhysicsConfig& config);
    void Shutdown();

    /// Creates Jolt bodies for entities that have RigidBody + Collider + Transform
    /// and do not yet have PhysicsBody.
    void CreateBodies(entt::registry& registry);

    /// Fixed-timestep simulation; writes dynamic body poses back to Transform.
    void Update(entt::registry& registry, float elapsedSeconds);

    bool IsInitialized() const;

    /// Drive / query dynamic bodies. No-op if id invalid / not added.
    void SetLinearVelocity(BodyId id, const Diligent::float3& velocity);
    /// Sets orientation from Euler degrees (Rx*Ry*Rz). Optionally clears angular velocity.
    void SetRotationDegrees(BodyId id, const Diligent::float3& eulerDegrees, bool resetAngularVelocity = true);
    /// Sets orientation from a unit quaternion (x,y,z,w). Preferred for flight-authored attitude.
    void SetRotation(BodyId id, const Diligent::QuaternionF& rotation, bool resetAngularVelocity = true);
    void SetGravityFactor(BodyId id, float factor);
    void AddForce(BodyId id, const Diligent::float3& force);
    /// World-space force applied at a world-space point (produces force + torque).
    void AddForceAtWorldPoint(BodyId id, const Diligent::float3& force, const Diligent::float3& worldPoint);
    /// World-space torque (N·m).
    void AddTorque(BodyId id, const Diligent::float3& torque);
    Diligent::float3 GetLinearVelocity(BodyId id);
    Diligent::float3 GetAngularVelocity(BodyId id);
    Diligent::float3 GetPosition(BodyId id);
    Diligent::QuaternionF GetRotation(BodyId id);
    void SetAngularVelocity(BodyId id, const Diligent::float3& angularVelocity);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace physics
} // namespace sapana
