#include "sapana/physics/PhysicsConfig.hpp"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

namespace sapana
{
namespace physics
{

bool PhysicsConfig::LoadFromFile(const char* path)
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

        if (root.contains("gravity") && root.at("gravity").is_array() && root.at("gravity").size() >= 3)
        {
            Gravity = Diligent::float3{
                root.at("gravity").at(0).get<float>(),
                root.at("gravity").at(1).get<float>(),
                root.at("gravity").at(2).get<float>()};
        }
        if (root.contains("fixed_dt") && root.at("fixed_dt").is_number())
            FixedDt = root.at("fixed_dt").get<float>();
        if (root.contains("max_substeps") && root.at("max_substeps").is_number_integer())
            MaxSubsteps = root.at("max_substeps").get<int>();

        if (FixedDt <= 0.f)
            FixedDt = 1.f / 60.f;
        if (MaxSubsteps < 1)
            MaxSubsteps = 1;
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "Sapana PhysicsConfig: parse error in [" << path << "]: " << ex.what() << '\n';
        return false;
    }

    return true;
}

} // namespace physics
} // namespace sapana
