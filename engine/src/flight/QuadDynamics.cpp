#include "sapana/flight/QuadDynamics.hpp"

#include "sapana/ecs/Components.hpp"
#include "sapana/physics/PhysicsComponents.hpp"

#include <algorithm>
#include <cmath>

namespace sapana
{
namespace flight
{

namespace
{

using Diligent::float3;
using Diligent::float4x4;
using Diligent::length;
using Diligent::normalize;
using Diligent::QuaternionF;
using Diligent::PI_F;

constexpr float kGravityY     = -9.81f;
constexpr float kClimbIdleEps = 0.5f;
constexpr float kEps          = 1e-6f;

QuaternionF QuatFromEulerDegrees(const float3& degrees)
{
    const float rx = degrees.x * (PI_F / 180.f);
    const float ry = degrees.y * (PI_F / 180.f);
    const float rz = degrees.z * (PI_F / 180.f);
    const QuaternionF qx = QuaternionF::RotationFromAxisAngle(float3{1.f, 0.f, 0.f}, rx);
    const QuaternionF qy = QuaternionF::RotationFromAxisAngle(float3{0.f, 1.f, 0.f}, ry);
    const QuaternionF qz = QuaternionF::RotationFromAxisAngle(float3{0.f, 0.f, 1.f}, rz);
    return normalize(qx * qy * qz);
}

QuaternionF IntegrateBodyRates(const QuaternionF& attitude, float pitchRate, float yawRate, float rollRate, float dt)
{
    QuaternionF dq{0.f, 0.f, 0.f, 1.f};
    if (std::fabs(pitchRate) > kEps)
        dq = dq * QuaternionF::RotationFromAxisAngle(float3{1.f, 0.f, 0.f}, pitchRate * dt);
    if (std::fabs(yawRate) > kEps)
        dq = dq * QuaternionF::RotationFromAxisAngle(float3{0.f, 1.f, 0.f}, yawRate * dt);
    if (std::fabs(rollRate) > kEps)
        dq = dq * QuaternionF::RotationFromAxisAngle(float3{0.f, 0.f, 1.f}, rollRate * dt);
    return normalize(attitude * dq);
}

QuaternionF LevelKeepYaw(const QuaternionF& attitude)
{
    const float3 forward = attitude.RotateVector(float3{0.f, 0.f, 1.f});
    float3       flat{forward.x, 0.f, forward.z};
    const float  len = length(flat);
    if (len < 1e-5f)
        return QuaternionF{0.f, 0.f, 0.f, 1.f};
    flat /= len;
    const float yaw = std::atan2(flat.x, flat.z);
    return QuaternionF::RotationFromAxisAngle(float3{0.f, 1.f, 0.f}, yaw);
}

float3 HeadingMoveWorld(const float3& headingLocal, const QuaternionF& attitude)
{
    const QuaternionF yawOnly = LevelKeepYaw(attitude);
    float3            dir     = headingLocal;
    dir.y                     = 0.f;
    const float len           = length(dir);
    if (len < 1e-5f)
        return float3{0.f, 0.f, 0.f};
    dir /= len;
    return yawOnly.RotateVector(dir);
}

float3 ClampLength(const float3& v, float maxLen)
{
    const float len = length(v);
    if (len <= maxLen || len < 1e-8f)
        return v;
    return v * (maxLen / len);
}

/// FPV: gravity always on. W/S = equal thrust on 4 motors along body up (idle = 0).
/// Stick rates tracked with world-space torque (no motor differential — that mixed
/// into a wrong body frame and exploded as soon as the craft tilted).
void ApplyFpvQuad(physics::PhysicsSystem& physics,
                  physics::BodyId         id,
                  physics::FlightMotor&   motor,
                  const FlightConfig&     config,
                  const FlightCommand&    command,
                  float                   mass)
{
    motor.HoldAltitudeActive = false;

    const QuaternionF attitude = physics.GetRotation(id);
    motor.Attitude            = attitude;
    motor.AttitudeInitialized = true;

    const float3 pos    = physics.GetPosition(id);
    const float3 vel    = physics.GetLinearVelocity(id);
    const float3 omegaW = physics.GetAngularVelocity(id);
    const float3 bodyUp = attitude.RotateVector(float3{0.f, 1.f, 0.f});
    const float  arm    = std::max(0.05f, config.MotorArmM);
    const float  weight = mass * (-kGravityY);

    const float collectiveN = command.Collective * config.FpvThrustToWeight * weight;
    if (std::fabs(collectiveN) > 1e-5f)
    {
        const float  each   = collectiveN * 0.25f;
        const float3 force  = bodyUp * each;
        const float3 locals[4] = {
            float3{-arm, 0.f, arm},
            float3{arm, 0.f, arm},
            float3{-arm, 0.f, -arm},
            float3{arm, 0.f, -arm},
        };
        for (const float3& local : locals)
            physics.AddForceAtWorldPoint(id, force, pos + attitude.RotateVector(local));
    }

    // Desired body rates → world. PD on world ω (stable at any attitude).
    const float3 desiredBody{command.PitchRate, command.YawRate, command.RollRate};
    const float3 desiredW = attitude.RotateVector(desiredBody);
    const float  inertia  = std::max(0.02f, mass * arm * arm);
    const float  kp       = std::max(0.1f, config.RateGain);
    const float  kd       = std::max(0.1f, config.AngularDrag);
    float3       torque   = inertia * (kp * (desiredW - omegaW) - kd * omegaW);
    torque                = ClampLength(torque, inertia * 40.f);
    if (length(torque) > 1e-6f)
        physics.AddTorque(id, torque);

    const float3 drag{-vel.x * config.LinearDrag, -vel.y * config.LinearDrag * 0.4f,
                      -vel.z * config.LinearDrag};
    if (std::fabs(drag.x) > 1e-6f || std::fabs(drag.y) > 1e-6f || std::fabs(drag.z) > 1e-6f)
        physics.AddForce(id, drag);
}

} // namespace

void QuadDynamics::Apply(entt::registry&         registry,
                         physics::PhysicsSystem& physics,
                         entt::entity            entity,
                         const FlightCommand&    command,
                         float                   dt)
{
    if (!physics.IsInitialized() || dt <= 0.f)
        return;
    if (!registry.valid(entity) ||
        !registry.all_of<physics::FlightMotor, physics::PhysicsBody, ecs::Transform>(entity))
        return;

    auto& motor = registry.get<physics::FlightMotor>(entity);
    if (!motor.Enabled)
        return;

    const physics::BodyId id = registry.get<physics::PhysicsBody>(entity).Id;
    if (id == physics::kInvalidBodyId)
        return;

    auto& transform = registry.get<ecs::Transform>(entity);

    float mass = 1.5f;
    if (registry.all_of<physics::RigidBody>(entity))
        mass = std::max(0.1f, registry.get<physics::RigidBody>(entity).Mass);

    if (command.Profile == FlightProfile::FPV)
    {
        ApplyFpvQuad(physics, id, motor, m_Config, command, mass);
        return;
    }

    // --- DJI: level attitude, world-up hover, altitude hold ---
    const float3 vel = physics.GetLinearVelocity(id);

    if (!motor.AttitudeInitialized)
    {
        motor.Attitude            = QuatFromEulerDegrees(transform.RotationDegrees);
        motor.AttitudeInitialized = true;
    }

    motor.Attitude = IntegrateBodyRates(motor.Attitude, 0.f, command.YawRate, 0.f, dt);
    motor.Attitude = LevelKeepYaw(motor.Attitude);

    transform.RotationQuat    = motor.Attitude;
    transform.UseRotationQuat = true;
    physics.SetRotation(id, motor.Attitude, true);

    const float hoverForce = mass * (-kGravityY) * m_Config.HoverThrust;
    float       throttleN  = hoverForce;
    if (std::fabs(command.ClimbRate) > kClimbIdleEps)
    {
        throttleN += command.ClimbRate * mass;
        motor.HoldAltitudeActive = false;
    }
    else if (command.Active)
    {
        if (!motor.HoldAltitudeActive)
        {
            motor.HoldAltitudeY      = transform.Position.y;
            motor.HoldAltitudeActive = true;
        }
        const float err = motor.HoldAltitudeY - transform.Position.y;
        throttleN += err * m_Config.AltitudeHoldStrength * mass;
        throttleN -= vel.y * m_Config.AltitudeHoldStrength * 0.35f * mass;
    }
    else
    {
        motor.HoldAltitudeActive = false;
    }

    float3 forceWorld{0.f, throttleN, 0.f};
    if (command.Active)
    {
        const float3 horiz = HeadingMoveWorld(command.HeadingMoveXZ, motor.Attitude);
        forceWorld += horiz * m_Config.HeadingAccel * mass;
    }

    forceWorld += float3{-vel.x * m_Config.LinearDrag, -vel.y * m_Config.LinearDrag * 0.4f,
                         -vel.z * m_Config.LinearDrag};

    if (std::fabs(forceWorld.x) > 1e-6f || std::fabs(forceWorld.y) > 1e-6f ||
        std::fabs(forceWorld.z) > 1e-6f)
        physics.AddForce(id, forceWorld);

    physics.SetAngularVelocity(id, float3{0.f, 0.f, 0.f});

    const float3 v2    = physics.GetLinearVelocity(id);
    const float  speed = length(v2);
    if (speed > m_Config.MaxSpeed && speed > 1e-5f)
        physics.SetLinearVelocity(id, (v2 / speed) * m_Config.MaxSpeed);
}

} // namespace flight
} // namespace sapana
