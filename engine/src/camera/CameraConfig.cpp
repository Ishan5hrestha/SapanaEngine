#include "sapana/camera/CameraConfig.hpp"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

namespace sapana
{
namespace camera
{

bool CameraConfig::LoadFromFile(const char* path)
{
    if (path == nullptr)
    {
        std::cerr << "Sapana CameraConfig: path is null\n";
        return false;
    }

    std::ifstream file(path);
    if (!file)
    {
        std::cerr << "Sapana CameraConfig: failed to open '" << path << "'\n";
        return false;
    }

    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "Sapana CameraConfig: JSON parse error in '" << path << "': " << ex.what() << '\n';
        return false;
    }

    // Only overwrite fields that are present; keep defaults for missing keys.
    if (root.contains("move_speed"))
        MoveSpeed = root.at("move_speed").get<float>();
    if (root.contains("look_sensitivity"))
        LookSensitivity = root.at("look_sensitivity").get<float>();
    if (root.contains("fov_y_degrees"))
        FovYDegrees = root.at("fov_y_degrees").get<float>();
    if (root.contains("near_plane"))
        NearPlane = root.at("near_plane").get<float>();
    if (root.contains("far_plane"))
        FarPlane = root.at("far_plane").get<float>();
    if (root.contains("pitch_limit_degrees"))
        PitchLimitDegrees = root.at("pitch_limit_degrees").get<float>();

    return true;
}

} // namespace camera
} // namespace sapana
