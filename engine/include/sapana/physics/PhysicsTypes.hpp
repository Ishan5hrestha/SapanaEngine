#pragma once

#include <cstdint>

namespace sapana
{
namespace physics
{

/// Opaque Jolt BodyID storage. Games must not interpret the bit pattern.
using BodyId = std::uint32_t;

inline constexpr BodyId kInvalidBodyId = 0xFFFFFFFFu;

enum class BodyType
{
    Static,
    Dynamic,
    Kinematic // Reserved: not created in v1 (falls back to Static with a warning).
};

enum class ShapeType
{
    Box,
    Plane,  // Finite ground: implemented as a thin box matching Transform.Scale XZ.
    Sphere  // Reserved for a later milestone.
};

} // namespace physics
} // namespace sapana
