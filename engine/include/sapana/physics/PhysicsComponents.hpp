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

/// Reusable force-based motor (authored via scene "motor" block).
struct ForceMotor
{
    float MaxSpeed         = 8.f;
    float HorizontalForce  = 25.f;
    float ThrustForce      = 40.f;
    float DownForce        = 15.f; // mild downward when MoveDown held
    float Drag             = 2.f;
    bool  Enabled          = true;
};

/// Per-frame input consumed by ForceMotorSystem (not authored in JSON).
struct MotorInput
{
    Diligent::float3 LocalMove{0.f, 0.f, 0.f}; // x=right, y=up hint, z=forward (camera-style)
    float            YawRadians = 0.f;
    bool             Thrust     = false; // Space / primary lift
    bool             Active     = false; // false → no drive forces (still may apply drag if Desired)
};

} // namespace physics
} // namespace sapana
