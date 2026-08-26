#pragma once

#include "sapana/flight/FlightConfig.hpp"
#include "sapana/flight/FlightTypes.hpp"
#include "sapana/physics/PhysicsSystem.hpp"

#include <entt/entt.hpp>

namespace sapana
{
namespace flight
{

/**
 * Applies FlightCommand to FlightMotor entities via 4 virtual motors → net force + torque.
 * DJI: level attitude, heading-frame move, altitude hold when climb idle.
 * FPV: hold-to-rate pitch/roll (release → rate 0), thrust along body up, soft max tilt.
 */
class QuadDynamics
{
public:
    void ApplyConfig(const FlightConfig& config) { m_Config = config; }
    const FlightConfig& GetConfig() const { return m_Config; }

    /// Integrate command for one entity this frame (before PhysicsSystem::Update).
    void Apply(entt::registry&              registry,
               physics::PhysicsSystem&      physics,
               entt::entity                 entity,
               const FlightCommand&         command,
               float                        dt);

private:
    FlightConfig m_Config;
};

} // namespace flight
} // namespace sapana
