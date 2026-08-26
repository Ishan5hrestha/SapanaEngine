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

} // namespace physics
} // namespace sapana
