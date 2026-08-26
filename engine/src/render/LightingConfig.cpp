#include "sapana/render/LightingConfig.hpp"

#include <fstream>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

namespace sapana
{
namespace render
{

namespace
{

using Diligent::float3;
using Diligent::float4;
using Diligent::length;
using Diligent::normalize;

float3 ReadFloat3(const nlohmann::json& arr, const float3& fallback)
{
    if (!arr.is_array() || arr.size() < 3)
        return fallback;
    return float3{arr.at(0).get<float>(), arr.at(1).get<float>(), arr.at(2).get<float>()};
}

float4 ReadFloat4(const nlohmann::json& arr, const float4& fallback)
{
    if (!arr.is_array() || arr.size() < 4)
        return fallback;
    return float4{
        arr.at(0).get<float>(),
        arr.at(1).get<float>(),
        arr.at(2).get<float>(),
        arr.at(3).get<float>()};
}

} // namespace

LightingConfig::LightingConfig()
{
    EnsureDefaultLight();
}

void LightingConfig::EnsureDefaultLight()
{
    if (!Lights.empty())
        return;

    LightDesc sun;
    sun.Type      = LightType::Directional;
    sun.Direction = normalize(float3{-0.4f, -1.f, -0.3f});
    sun.Color     = float3{1.f, 1.f, 1.f};
    sun.Intensity = 3.f;
    Lights.push_back(sun);
}

bool LightingConfig::LoadFromFile(const char* path)
{
    if (path == nullptr)
    {
        std::cerr << "Sapana LightingConfig: path is null\n";
        return false;
    }

    std::ifstream file(path);
    if (!file)
    {
        std::cerr << "Sapana LightingConfig: failed to open '" << path << "'\n";
        return false;
    }

    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "Sapana LightingConfig: JSON parse error in '" << path << "': " << ex.what() << '\n';
        return false;
    }

    if (!root.is_object())
    {
        std::cerr << "Sapana LightingConfig: root must be an object in '" << path << "'\n";
        return false;
    }

    LightingConfig parsed = *this;

    try
    {
        if (root.contains("clear_color"))
            parsed.ClearColor = ReadFloat4(root.at("clear_color"), parsed.ClearColor);

        if (root.contains("enable_ibl") && root.at("enable_ibl").is_boolean())
            parsed.EnableIbl = root.at("enable_ibl").get<bool>();

        if (root.contains("sky") && root.at("sky").is_object())
        {
            const auto& sky = root.at("sky");
            if (sky.contains("enabled") && sky.at("enabled").is_boolean())
                parsed.Sky.Enabled = sky.at("enabled").get<bool>();
            if (sky.contains("zenith_color"))
                parsed.Sky.ZenithColor = ReadFloat3(sky.at("zenith_color"), parsed.Sky.ZenithColor);
            if (sky.contains("horizon_color"))
                parsed.Sky.HorizonColor = ReadFloat3(sky.at("horizon_color"), parsed.Sky.HorizonColor);
            if (sky.contains("ground_color"))
                parsed.Sky.GroundColor = ReadFloat3(sky.at("ground_color"), parsed.Sky.GroundColor);
            if (sky.contains("horizon_falloff") && sky.at("horizon_falloff").is_number())
            {
                parsed.Sky.HorizonFalloff = sky.at("horizon_falloff").get<float>();
                if (parsed.Sky.HorizonFalloff < 0.01f)
                    parsed.Sky.HorizonFalloff = 0.01f;
            }
        }

        if (root.contains("shadows") && root.at("shadows").is_object())
        {
            const auto& sh = root.at("shadows");
            if (sh.contains("enabled") && sh.at("enabled").is_boolean())
                parsed.Shadows.Enabled = sh.at("enabled").get<bool>();
            if (sh.contains("resolution") && sh.at("resolution").is_number_integer())
            {
                parsed.Shadows.Resolution = sh.at("resolution").get<int>();
                if (parsed.Shadows.Resolution != 512 && parsed.Shadows.Resolution != 1024 &&
                    parsed.Shadows.Resolution != 2048 && parsed.Shadows.Resolution != 4096)
                    parsed.Shadows.Resolution = 2048;
            }
            if (sh.contains("cascades") && sh.at("cascades").is_number_integer())
            {
                parsed.Shadows.Cascades = sh.at("cascades").get<int>();
                if (parsed.Shadows.Cascades < 1)
                    parsed.Shadows.Cascades = 1;
                if (parsed.Shadows.Cascades > 2)
                    parsed.Shadows.Cascades = 2;
            }
            if (sh.contains("pcf_kernel") && sh.at("pcf_kernel").is_number_integer())
            {
                parsed.Shadows.PcfKernel = sh.at("pcf_kernel").get<int>();
                if (parsed.Shadows.PcfKernel != 2 && parsed.Shadows.PcfKernel != 3 &&
                    parsed.Shadows.PcfKernel != 5 && parsed.Shadows.PcfKernel != 7)
                    parsed.Shadows.PcfKernel = 3;
            }
            if (sh.contains("max_distance") && sh.at("max_distance").is_number())
            {
                parsed.Shadows.MaxDistance = sh.at("max_distance").get<float>();
                if (parsed.Shadows.MaxDistance < 1.f)
                    parsed.Shadows.MaxDistance = 1.f;
            }
            if (sh.contains("depth_bias") && sh.at("depth_bias").is_number())
                parsed.Shadows.DepthBias = sh.at("depth_bias").get<float>();
        }

        if (root.contains("tone_mapping") && root.at("tone_mapping").is_object())
        {
            const auto& tm = root.at("tone_mapping");
            if (tm.contains("average_log_lum") && tm.at("average_log_lum").is_number())
                parsed.AverageLogLum = tm.at("average_log_lum").get<float>();
            if (tm.contains("middle_gray") && tm.at("middle_gray").is_number())
                parsed.MiddleGray = tm.at("middle_gray").get<float>();
            if (tm.contains("white_point") && tm.at("white_point").is_number())
                parsed.WhitePoint = tm.at("white_point").get<float>();
        }

        if (root.contains("lights") && root.at("lights").is_array())
        {
            parsed.Lights.clear();
            for (const auto& entry : root.at("lights"))
            {
                if (!entry.is_object())
                    continue;
                if (static_cast<int>(parsed.Lights.size()) >= kMaxConfiguredLights)
                {
                    std::cerr << "Sapana LightingConfig: ignoring lights beyond " << kMaxConfiguredLights << '\n';
                    break;
                }

                std::string typeStr = "directional";
                if (entry.contains("type") && entry.at("type").is_string())
                    typeStr = entry.at("type").get<std::string>();

                if (typeStr != "directional")
                {
                    std::cerr << "Sapana LightingConfig: ignoring unsupported light type '" << typeStr << "'\n";
                    continue;
                }

                LightDesc light;
                light.Type = LightType::Directional;
                if (entry.contains("direction"))
                    light.Direction = ReadFloat3(entry.at("direction"), light.Direction);
                if (entry.contains("color"))
                    light.Color = ReadFloat3(entry.at("color"), light.Color);
                if (entry.contains("intensity") && entry.at("intensity").is_number())
                    light.Intensity = entry.at("intensity").get<float>();

                const float dirLen = length(light.Direction);
                if (dirLen < 1e-5f)
                {
                    std::cerr << "Sapana LightingConfig: skipping directional light with zero direction\n";
                    continue;
                }
                light.Direction = light.Direction / dirLen;

                if (light.Intensity < 0.f)
                    light.Intensity = 0.f;

                parsed.Lights.push_back(light);
            }
        }
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "Sapana LightingConfig: field error in '" << path << "': " << ex.what() << '\n';
        return false;
    }

    parsed.EnsureDefaultLight();
    *this = std::move(parsed);
    return true;
}

} // namespace render
} // namespace sapana
