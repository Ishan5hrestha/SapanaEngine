#pragma once

#include "sapana/input/Action.hpp"

#include <array>
#include <string>
#include <vector>

namespace sapana
{
namespace input
{

/// Binding tokens from JSON, e.g. "Diligent.MoveForward", "Mouse.Delta", "Key.M".
using BindingList = std::vector<std::string>;

/// Maps each Action to zero or more binding tokens.
class InputBindings
{
public:
    InputBindings();

    /// Load from JSON file. On failure, restores built-in defaults and returns false.
    bool LoadFromFile(const char* path);

    void ResetToDefaults();

    const BindingList& GetBindings(Action action) const;

private:
    std::array<BindingList, static_cast<std::size_t>(Action::Count)> m_Bindings;
};

} // namespace input
} // namespace sapana
