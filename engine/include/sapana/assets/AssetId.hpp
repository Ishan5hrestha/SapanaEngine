#pragma once

#include <string>

namespace sapana
{
namespace assets
{

/// Stable asset identifier (e.g. "builtin:cube" or "meshes/foo.glb").
class AssetId
{
public:
    AssetId() = default;
    explicit AssetId(std::string id) :
        m_Id{std::move(id)}
    {
    }

    const std::string& Str() const { return m_Id; }
    bool               Empty() const { return m_Id.empty(); }

    bool operator==(const AssetId& other) const { return m_Id == other.m_Id; }
    bool operator!=(const AssetId& other) const { return m_Id != other.m_Id; }
    bool operator<(const AssetId& other) const { return m_Id < other.m_Id; }

private:
    std::string m_Id;
};

inline constexpr const char* kBuiltinCubeId  = "builtin:cube";
inline constexpr const char* kBuiltinPlaneId = "builtin:plane";

} // namespace assets
} // namespace sapana
