#include "sapana/flight/FlightController.hpp"

#include "sapana/input/Action.hpp"

#include <cmath>

namespace sapana
{
namespace flight
{

namespace
{

using Diligent::float3;
using Diligent::PI_F;

float AxisFromKeys(const input::InputSystem& input, input::Action pos, input::Action neg)
{
    float v = 0.f;
    if (input.IsDown(pos))
        v += 1.f;
    if (input.IsDown(neg))
        v -= 1.f;
    return v;
}

} // namespace

FlightCommand FlightController::BuildCommand(const input::InputSystem& input, const FlightConfig& config) const
{
    FlightCommand cmd;
    cmd.Active  = true;
    cmd.Profile = m_Profile;

    const float climb = AxisFromKeys(input, input::Action::FlightClimbUp, input::Action::FlightClimbDown);
    const float yaw   = AxisFromKeys(input, input::Action::FlightYawLeft, input::Action::FlightYawRight);
    const float axisV = AxisFromKeys(input, input::Action::FlightAxisUp, input::Action::FlightAxisDown);
    const float axisH = AxisFromKeys(input, input::Action::FlightAxisLeft, input::Action::FlightAxisRight);

    const float yawRateRad   = config.YawRateDeg * (PI_F / 180.f);
    const float pitchRateRad = config.PitchRateDeg * (PI_F / 180.f);
    const float rollRateRad  = config.RollRateDeg * (PI_F / 180.f);

    // W/S → climb, A/D → yaw (A = left / D = right from the pilot).
    cmd.ClimbRate  = climb * config.ClimbAccel;
    cmd.Collective = climb;
    cmd.YawRate    = -yaw * yawRateRad;

    if (m_Profile == FlightProfile::DJI)
    {
        // Arrows: heading-frame translate; body stays level (no pitch/roll rates).
            cmd.HeadingMoveXZ = float3{-axisH, 0.f, axisV};
        cmd.PitchRate     = 0.f;
        cmd.RollRate      = 0.f;
    }
    else
    {
        // FPV hold-to-rate: ↑/↓ pitch, ←/→ roll; release → rate 0 (attitude stays).
        cmd.PitchRate     = axisV * pitchRateRad;
        cmd.RollRate      = axisH * rollRateRad;
        cmd.HeadingMoveXZ = float3{0.f, 0.f, 0.f};
    }

    return cmd;
}

} // namespace flight
} // namespace sapana
