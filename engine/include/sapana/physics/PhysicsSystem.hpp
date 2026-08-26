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

    /// Drive a dynamic body (e.g. player drone). No-op if id invalid / not added.
    void SetLinearVelocity(BodyId id, const Diligent::float3& velocity);
    void SetRotationDegrees(BodyId id, const Diligent::float3& eulerDegrees);
    void SetGravityFactor(BodyId id, float factor);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace physics
} // namespace sapana
