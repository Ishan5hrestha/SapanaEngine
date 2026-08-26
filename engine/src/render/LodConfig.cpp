#include "sapana/render/LodConfig.hpp"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

namespace sapana
{
namespace render
{

const MeshLodEntry* LodConfig::FindMeshEntry(const assets::AssetId& baseId) const
{
    if (baseId.Empty())
        return nullptr;
    const auto it = Meshes.find(baseId.Str());
    if (it == Meshes.end())
        return nullptr;
    return &it->second;
}

bool LodConfig::LoadFromFile(const char* path)
{
    if (path == nullptr)
    {
        std::cerr << "Sapana LodConfig: path is null\n";
        return false;
    }

    std::ifstream file(path);
    if (!file)
    {
        std::cerr << "Sapana LodConfig: failed to open '" << path << "'\n";
        return false;
    }

    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "Sapana LodConfig: JSON parse error in '" << path << "': " << ex.what() << '\n';
        return false;
    }

    if (!root.is_object())
    {
        std::cerr << "Sapana LodConfig: root must be an object in '" << path << "'\n";
        return false;
    }

    LodConfig parsed = *this;

    try
    {
        if (root.contains("enabled") && root.at("enabled").is_boolean())
            parsed.Enabled = root.at("enabled").get<bool>();
        if (root.contains("frustum_culling") && root.at("frustum_culling").is_boolean())
            parsed.FrustumCulling = root.at("frustum_culling").get<bool>();
        if (root.contains("debug_stats") && root.at("debug_stats").is_boolean())
            parsed.DebugStats = root.at("debug_stats").get<bool>();

        if (root.contains("far_cull_distance") && root.at("far_cull_distance").is_number())
            parsed.FarCullDistance = root.at("far_cull_distance").get<float>();
        if (root.contains("lod1_distance") && root.at("lod1_distance").is_number())
            parsed.Lod1Distance = root.at("lod1_distance").get<float>();
        if (root.contains("hysteresis") && root.at("hysteresis").is_number())
            parsed.Hysteresis = root.at("hysteresis").get<float>();
        if (root.contains("shadow_far_cull_distance") && root.at("shadow_far_cull_distance").is_number())
            parsed.ShadowFarCullDistance = root.at("shadow_far_cull_distance").get<float>();
        if (root.contains("shadow_lod1_distance") && root.at("shadow_lod1_distance").is_number())
            parsed.ShadowLod1Distance = root.at("shadow_lod1_distance").get<float>();
        if (root.contains("default_bounding_radius") && root.at("default_bounding_radius").is_number())
            parsed.DefaultBoundingRadius = root.at("default_bounding_radius").get<float>();

        if (parsed.FarCullDistance < 1.f)
            parsed.FarCullDistance = 1.f;
        if (parsed.ShadowFarCullDistance < 1.f)
            parsed.ShadowFarCullDistance = 1.f;
        if (parsed.Lod1Distance < 0.f)
            parsed.Lod1Distance = 0.f;
        if (parsed.ShadowLod1Distance < 0.f)
            parsed.ShadowLod1Distance = 0.f;
        if (parsed.Hysteresis < 0.f)
            parsed.Hysteresis = 0.f;
        if (parsed.DefaultBoundingRadius < 0.1f)
            parsed.DefaultBoundingRadius = 0.1f;

        parsed.Meshes.clear();
        if (root.contains("meshes") && root.at("meshes").is_object())
        {
            for (auto it = root.at("meshes").begin(); it != root.at("meshes").end(); ++it)
            {
                if (!it.value().is_object())
                    continue;

                MeshLodEntry entry;
                const auto&  meshJson = it.value();
                if (meshJson.contains("lod1") && meshJson.at("lod1").is_string())
                    entry.Lod1 = assets::AssetId{meshJson.at("lod1").get<std::string>()};
                if (meshJson.contains("bounding_radius") && meshJson.at("bounding_radius").is_number())
                {
                    entry.BoundingRadius = meshJson.at("bounding_radius").get<float>();
                    if (entry.BoundingRadius < 0.1f)
                        entry.BoundingRadius = 0.1f;
                }
                parsed.Meshes.emplace(it.key(), std::move(entry));
            }
        }
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "Sapana LodConfig: invalid fields in '" << path << "': " << ex.what() << '\n';
        return false;
    }

    *this = std::move(parsed);
    return true;
}

} // namespace render
} // namespace sapana
