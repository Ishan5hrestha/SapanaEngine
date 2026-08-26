#pragma once

#include <cstddef>

namespace sapana
{
namespace input
{

/// Logical gameplay actions. Bindings map these to hardware in input_bindings.json.
enum class Action : std::size_t
{
    MoveForward = 0,
    MoveBackward,
    MoveLeft,
    MoveRight,
    MoveUp,
    MoveDown,
    Look,
    ToggleCursor,
    ToggleControlMode, // K — cycles camera (Freelook / Chase / FpvNose)
    Thrust,
    ToggleFlightMode, // L — DJI ↔ FPV
    /// Drone sticks (Hold.* / Key.Up so WASD ≠ arrows; Diligent maps both to Move*).
    FlightClimbUp,
    FlightClimbDown,
    FlightYawLeft,
    FlightYawRight,
    FlightAxisUp,
    FlightAxisDown,
    FlightAxisLeft,
    FlightAxisRight,
    Count
};

/// Human-readable action name used as JSON object keys.
const char* ActionToString(Action action);

/// Parse action name from bindings JSON. Returns false if unknown.
bool TryParseAction(const char* name, Action& outAction);

} // namespace input
} // namespace sapana
