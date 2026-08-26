#pragma once

#include "BasicMath.hpp"
#include "sapana/physics/PhysicsTypes.hpp"

namespace sapana
{
namespace physics
{

/// Authored rigid-body parameters (from scene JSON).
struct RigidBody
{
    BodyType Type         = BodyType::Static;
    float    Mass         = 1.f;
    float    Friction     = 0.5f;
    float    Restitution  = 0.f;
};

/// Collision shape description. If HalfExtents is (0,0,0), PhysicsSystem derives
/// extents from Transform.Scale (box ≈ scale; plane ≈ thin box on XZ).
struct Collider
{
    ShapeType         Shape       = ShapeType::Box;
    Diligent::float3  HalfExtents{0.f, 0.f, 0.f};
};

/// Runtime handle created by PhysicsSystem — not authored in JSON.
struct PhysicsBody
{
    BodyId Id = kInvalidBodyId;
};

/// Reusable force-based motor (authored via scene "motor" block) — tank/simple props.
struct ForceMotor
{
    float MaxSpeed         = 8.f;
    float HorizontalForce  = 25.f;
    float ThrustForce      = 40.f;
    float DownForce        = 15.f;
    float Drag             = 2.f;
    bool  Enabled          = true;
};

/// Marks a dynamic body as a quadcopter flight craft (full 6-DOF).
/// DJI: QuadDynamics authors Attitude and snaps Jolt. FPV: Jolt owns Attitude (4-motor forces).
struct FlightMotor
{
    bool Enabled = true;
    /// Authoritative orientation; integrated by QuadDynamics (not Jolt euler extract).
    Diligent::QuaternionF Attitude{0.f, 0.f, 0.f, 1.f};
    bool                  AttitudeInitialized = false;
    /// Runtime altitude-hold target (world Y). Valid when HoldAltitudeActive.
    float HoldAltitudeY      = 0.f;
    bool  HoldAltitudeActive = false;
};

/// Per-frame input consumed by ForceMotorSystem (not authored in JSON).
struct MotorInput
{
    Diligent::float3 LocalMove{0.f, 0.f, 0.f};
    float            YawRadians = 0.f;
    bool             Thrust     = false;
    bool             Active     = false;
};

} // namespace physics
} // namespace sapana
