#include "sapana/input/Action.hpp"

#include <cstring>

namespace sapana
{
namespace input
{

const char* ActionToString(Action action)
{
    switch (action)
    {
        case Action::MoveForward:       return "MoveForward";
        case Action::MoveBackward:      return "MoveBackward";
        case Action::MoveLeft:          return "MoveLeft";
        case Action::MoveRight:         return "MoveRight";
        case Action::MoveUp:            return "MoveUp";
        case Action::MoveDown:          return "MoveDown";
        case Action::Look:              return "Look";
        case Action::ToggleCursor:      return "ToggleCursor";
        case Action::ToggleControlMode: return "ToggleControlMode";
        case Action::Thrust:            return "Thrust";
        case Action::ToggleFlightMode:  return "ToggleFlightMode";
        case Action::FlightClimbUp:     return "FlightClimbUp";
        case Action::FlightClimbDown:   return "FlightClimbDown";
        case Action::FlightYawLeft:     return "FlightYawLeft";
        case Action::FlightYawRight:    return "FlightYawRight";
        case Action::FlightAxisUp:      return "FlightAxisUp";
        case Action::FlightAxisDown:    return "FlightAxisDown";
        case Action::FlightAxisLeft:    return "FlightAxisLeft";
        case Action::FlightAxisRight:   return "FlightAxisRight";
        case Action::Count:             break;
    }
    return "Unknown";
}

bool TryParseAction(const char* name, Action& outAction)
{
    if (name == nullptr)
        return false;

    for (std::size_t i = 0; i < static_cast<std::size_t>(Action::Count); ++i)
    {
        const Action action = static_cast<Action>(i);
        if (std::strcmp(name, ActionToString(action)) == 0)
        {
            outAction = action;
            return true;
        }
    }
    return false;
}

} // namespace input
} // namespace sapana
