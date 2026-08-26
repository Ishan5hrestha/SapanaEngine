#pragma once

#include "BasicMath.hpp"

#include <string>

namespace sapana
{
namespace physics
{

/// Tunables for the physics world (config/physics.json).
struct PhysicsConfig
{
    Diligent::float3 Gravity{0.f, -9.81f, 0.f};
    float            FixedDt     = 1.f / 60.f;
    int              MaxSubsteps = 5;

    /// Loads JSON from path. Missing/invalid file keeps defaults and returns false.
    bool LoadFromFile(const char* path);
};

} // namespace physics
} // namespace sapana
