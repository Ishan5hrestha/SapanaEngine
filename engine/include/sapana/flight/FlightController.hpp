#pragma once

#include "sapana/flight/FlightConfig.hpp"
#include "sapana/flight/FlightTypes.hpp"
#include "sapana/input/InputSystem.hpp"

namespace sapana
{
namespace flight
{

/// Maps keyboard actions → FlightCommand for the active profile.
class FlightController
{
public:
    void SetProfile(FlightProfile profile) { m_Profile = profile; }
    FlightProfile GetProfile() const { return m_Profile; }

    void ToggleProfile()
    {
        m_Profile = (m_Profile == FlightProfile::DJI) ? FlightProfile::FPV : FlightProfile::DJI;
    }

    /// Build a command for this frame. Call only when a drone camera is active.
    FlightCommand BuildCommand(const input::InputSystem& input, const FlightConfig& config) const;

private:
    FlightProfile m_Profile = FlightProfile::DJI;
};

} // namespace flight
} // namespace sapana
