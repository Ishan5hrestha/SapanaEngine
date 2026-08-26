#include "sapana/flight/FlightConfig.hpp"

#include <fstream>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

namespace sapana
{
namespace flight
{

bool FlightConfig::LoadFromFile(const char* path)
{
    if (path == nullptr)
        return false;

    std::ifstream file(path);
    if (!file)
        return false;

    try
    {
        nlohmann::json root;
        file >> root;
        if (!root.is_object())
            return false;

        if (root.contains("default_profile") && root.at("default_profile").is_string())
        {
            const std::string p = root.at("default_profile").get<std::string>();
            if (p == "fpv" || p == "FPV")
                DefaultProfile = FlightProfile::FPV;
            else
                DefaultProfile = FlightProfile::DJI;
        }

        auto readF = [&](const char* key, float& dst) {
            if (root.contains(key) && root.at(key).is_number())
                dst = root.at(key).get<float>();
        };

        readF("yaw_rate_deg", YawRateDeg);
        readF("pitch_rate_deg", PitchRateDeg);
        readF("roll_rate_deg", RollRateDeg);
        readF("max_tilt_deg", MaxTiltDeg);
        readF("hover_thrust", HoverThrust);
        readF("climb_accel", ClimbAccel);
        readF("heading_accel", HeadingAccel);
        readF("altitude_hold_strength", AltitudeHoldStrength);
        readF("motor_arm_m", MotorArmM);
        readF("linear_drag", LinearDrag);
        readF("angular_drag", AngularDrag);
        readF("max_speed", MaxSpeed);
        readF("fpv_thrust_to_weight", FpvThrustToWeight);
        readF("rate_gain", RateGain);
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "Sapana FlightConfig: parse error in [" << path << "]: " << ex.what() << '\n';
        return false;
    }

    return true;
}

} // namespace flight
} // namespace sapana
