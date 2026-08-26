#include "sapana/scene/SceneLoader.hpp"

#include "sapana/ecs/Components.hpp"
#include "sapana/physics/PhysicsComponents.hpp"

#include <fstream>
#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

namespace sapana
{
namespace scene
{

namespace
{

using Diligent::float3;
using Diligent::float4;

float3 ReadFloat3(const nlohmann::json& arr, const float3& fallback)
{
    if (!arr.is_array() || arr.size() < 3)
        return fallback;
    return float3{
        arr.at(0).get<float>(),
        arr.at(1).get<float>(),
        arr.at(2).get<float>()};
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

void LoadPhysicsBlock(const nlohmann::json& entJson, entt::entity entity, entt::registry& registry)
{
    if (!entJson.contains("physics") || !entJson.at("physics").is_object())
        return;

    const auto& p = entJson.at("physics");
    physics::RigidBody rigid;
    physics::Collider  collider;

    if (p.contains("body") && p.at("body").is_string())
    {
        const std::string body = p.at("body").get<std::string>();
        if (body == "dynamic")
            rigid.Type = physics::BodyType::Dynamic;
        else if (body == "kinematic")
            rigid.Type = physics::BodyType::Kinematic;
        else if (body == "static")
            rigid.Type = physics::BodyType::Static;
        else
            std::cerr << "Sapana SceneLoader: unknown physics.body '" << body << "', using static\n";
    }

    if (p.contains("shape") && p.at("shape").is_string())
    {
        const std::string shape = p.at("shape").get<std::string>();
        if (shape == "plane")
            collider.Shape = physics::ShapeType::Plane;
        else if (shape == "sphere")
            collider.Shape = physics::ShapeType::Sphere;
        else if (shape == "box")
            collider.Shape = physics::ShapeType::Box;
        else
            std::cerr << "Sapana SceneLoader: unknown physics.shape '" << shape << "', using box\n";
    }

    if (p.contains("mass") && p.at("mass").is_number())
        rigid.Mass = p.at("mass").get<float>();
    if (p.contains("friction") && p.at("friction").is_number())
        rigid.Friction = p.at("friction").get<float>();
    if (p.contains("restitution") && p.at("restitution").is_number())
        rigid.Restitution = p.at("restitution").get<float>();

    if (p.contains("half_extents"))
        collider.HalfExtents = ReadFloat3(p.at("half_extents"), collider.HalfExtents);

    for (auto it = p.begin(); it != p.end(); ++it)
    {
        const std::string& key = it.key();
        if (key != "body" && key != "shape" && key != "mass" && key != "friction" &&
            key != "restitution" && key != "half_extents")
        {
            std::cerr << "Sapana SceneLoader: ignoring unknown physics field '" << key << "'\n";
        }
    }

    registry.emplace<physics::RigidBody>(entity, rigid);
    registry.emplace<physics::Collider>(entity, collider);
}

void LoadMotorBlock(const nlohmann::json& entJson, entt::entity entity, entt::registry& registry)
{
    if (!entJson.contains("motor") || !entJson.at("motor").is_object())
        return;

    const auto& m = entJson.at("motor");
    physics::FlightMotor flight;
    if (m.contains("enabled") && m.at("enabled").is_boolean())
        flight.Enabled = m.at("enabled").get<bool>();

    for (auto it = m.begin(); it != m.end(); ++it)
    {
        const std::string& key = it.key();
        // Legacy ForceMotor field names are accepted and ignored (tunables live in flight.json).
        if (key != "max_speed" && key != "horizontal_force" && key != "thrust_force" &&
            key != "down_force" && key != "drag" && key != "enabled")
        {
            std::cerr << "Sapana SceneLoader: ignoring unknown motor field '" << key << "'\n";
        }
    }

    registry.emplace<physics::FlightMotor>(entity, flight);
}

} // namespace

bool SceneLoader::Load(const char*              path,
                       entt::registry&          registry,
                       assets::AssetCache&      assetCache,
                       const render::LodConfig* lodConfig)
{
    if (path == nullptr)
    {
        std::cerr << "Sapana SceneLoader: path is null\n";
        return false;
    }

    std::ifstream file(path);
    if (!file)
    {
        std::cerr << "Sapana SceneLoader: failed to open '" << path << "'\n";
        return false;
    }

    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "Sapana SceneLoader: JSON parse error in '" << path << "': " << ex.what() << '\n';
        return false;
    }

    if (!root.is_object())
    {
        std::cerr << "Sapana SceneLoader: root must be an object\n";
        return false;
    }

    if (root.contains("entities") && root.at("entities").is_array())
    {
        for (const auto& entJson : root.at("entities"))
        {
            if (!entJson.is_object())
                continue;

            const auto entity = registry.create();

            ecs::Name nameComp;
            if (entJson.contains("name") && entJson.at("name").is_string())
                nameComp.Value = entJson.at("name").get<std::string>();
            else
                nameComp.Value = "Entity";
            registry.emplace<ecs::Name>(entity, std::move(nameComp));

            ecs::Transform transform;
            if (entJson.contains("transform") && entJson.at("transform").is_object())
            {
                const auto& t = entJson.at("transform");
                if (t.contains("position"))
                    transform.Position = ReadFloat3(t.at("position"), transform.Position);
                if (t.contains("rotation_degrees"))
                    transform.RotationDegrees = ReadFloat3(t.at("rotation_degrees"), transform.RotationDegrees);
                if (t.contains("scale"))
                    transform.Scale = ReadFloat3(t.at("scale"), transform.Scale);
            }
            registry.emplace<ecs::Transform>(entity, transform);

            if (entJson.contains("mesh") && entJson.at("mesh").is_object())
            {
                const auto& m = entJson.at("mesh");
                ecs::MeshRenderer renderer;
                if (m.contains("asset") && m.at("asset").is_string())
                    renderer.MeshId = assets::AssetId{m.at("asset").get<std::string>()};
                if (m.contains("color"))
                    renderer.Color = ReadFloat4(m.at("color"), renderer.Color);

                if (!renderer.MeshId.Empty())
                {
                    if (assetCache.GetOrLoad(renderer.MeshId) == nullptr)
                    {
                        std::cerr << "Sapana SceneLoader: failed to load mesh '" << renderer.MeshId.Str()
                                  << "' for entity '" << registry.get<ecs::Name>(entity).Value << "'\n";
                    }
                    else
                    {
                        const assets::AssetId baseMeshId = renderer.MeshId;
                        registry.emplace<ecs::MeshRenderer>(entity, std::move(renderer));
                        registry.emplace<ecs::Visibility>(entity);

                        if (lodConfig != nullptr)
                        {
                            const auto* entry = lodConfig->FindMeshEntry(baseMeshId);
                            if (entry != nullptr)
                            {
                                ecs::LodGroup lod;
                                lod.BaseMeshId     = baseMeshId;
                                lod.Lod1MeshId     = entry->Lod1;
                                lod.BoundingRadius = entry->BoundingRadius;
                                if (!lod.Lod1MeshId.Empty())
                                {
                                    if (assetCache.GetOrLoad(lod.Lod1MeshId) == nullptr)
                                    {
                                        std::cerr << "Sapana SceneLoader: failed to preload LOD1 '"
                                                  << lod.Lod1MeshId.Str() << "' for '"
                                                  << registry.get<ecs::Name>(entity).Value << "'\n";
                                        lod.Lod1MeshId = assets::AssetId{};
                                    }
                                }
                                registry.emplace<ecs::LodGroup>(entity, std::move(lod));
                            }
                        }
                    }
                }
            }

            LoadPhysicsBlock(entJson, entity, registry);
            LoadMotorBlock(entJson, entity, registry);
        }
    }

    return true;
}

} // namespace scene
} // namespace sapana
