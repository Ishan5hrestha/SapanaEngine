#pragma once

#include "BasicMath.hpp"

namespace sapana
{
namespace flight
{

/// High-level stick profile for the craft.
enum class FlightProfile
{
    DJI = 0,
    FPV
};

/// Normalized command produced by FlightController each frame.
struct FlightCommand
{
    FlightProfile Profile = FlightProfile::DJI;

    /// Vertical intent scale (−1..1 * ClimbAccel in controller). DJI idle → altitude hold.
    float ClimbRate = 0.f;
    /// FPV collective −1..1 (W/S). 0 = motors idle (gravity only).
    float Collective = 0.f;
    /// Yaw rate in rad/s (world Y).
    float YawRate = 0.f;
    /// FPV: pitch rate rad/s. Unused in DJI (body stays level).
    float PitchRate = 0.f;
    /// FPV: roll rate rad/s. Unused in DJI.
    float RollRate = 0.f;
    /// DJI: desired accel direction in heading frame (x=strafe, z=forward). Unused in FPV.
    Diligent::float3 HeadingMoveXZ{0.f, 0.f, 0.f};
    bool Active = false;
};

} // namespace flight
} // namespace sapana
