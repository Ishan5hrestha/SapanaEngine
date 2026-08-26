#pragma once

#include "sapana/flight/FlightTypes.hpp"

namespace sapana
{
namespace flight
{

/// Tunables loaded from config/flight.json.
struct FlightConfig
{
    FlightProfile DefaultProfile = FlightProfile::DJI;

    float YawRateDeg   = 90.f;
    float PitchRateDeg = 120.f;
    float RollRateDeg  = 120.f;
    float MaxTiltDeg   = 75.f;

    float HoverThrust           = 1.0f; // multiplier vs mass*g for hover
    float ClimbAccel            = 12.f;
    float HeadingAccel          = 20.f;
    float AltitudeHoldStrength  = 8.f;
    float MotorArmM             = 0.25f;
    float LinearDrag            = 2.0f;
    float AngularDrag           = 4.0f;
    float MaxSpeed              = 16.f;
    /// FPV: full W thrust as a multiple of weight. Idle (no W) = 0 thrust, gravity wins.
    float FpvThrustToWeight     = 2.5f;
    /// FPV: body-rate tracking gain (hold-to-rate).
    float RateGain              = 12.f;

    bool LoadFromFile(const char* path);
};

} // namespace flight
} // namespace sapana
