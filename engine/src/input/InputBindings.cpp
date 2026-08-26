#include "sapana/input/InputBindings.hpp"

#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

namespace sapana
{
namespace input
{

InputBindings::InputBindings()
{
    ResetToDefaults();
}

void InputBindings::ResetToDefaults()
{
    m_Bindings[static_cast<std::size_t>(Action::MoveForward)]  = {"Diligent.MoveForward"};
    m_Bindings[static_cast<std::size_t>(Action::MoveBackward)] = {"Diligent.MoveBackward"};
    m_Bindings[static_cast<std::size_t>(Action::MoveLeft)]     = {"Diligent.MoveLeft"};
    m_Bindings[static_cast<std::size_t>(Action::MoveRight)]    = {"Diligent.MoveRight"};
    m_Bindings[static_cast<std::size_t>(Action::MoveUp)]       = {"Diligent.MoveUp"};
    m_Bindings[static_cast<std::size_t>(Action::MoveDown)]     = {"Diligent.MoveDown"};
    m_Bindings[static_cast<std::size_t>(Action::Look)]         = {"Mouse.Delta"};
    m_Bindings[static_cast<std::size_t>(Action::ToggleCursor)] = {"Key.M"};
}

bool InputBindings::LoadFromFile(const char* path)
{
    if (path == nullptr)
    {
        std::cerr << "Sapana InputBindings: path is null\n";
        return false;
    }

    std::ifstream file(path);
    if (!file)
    {
        std::cerr << "Sapana InputBindings: failed to open '" << path << "'\n";
        return false;
    }

    nlohmann::json root;
    try
    {
        file >> root;
    }
    catch (const nlohmann::json::exception& ex)
    {
        std::cerr << "Sapana InputBindings: JSON parse error in '" << path << "': " << ex.what() << '\n';
        return false;
    }

    if (!root.is_object())
    {
        std::cerr << "Sapana InputBindings: root must be a JSON object\n";
        return false;
    }

    ResetToDefaults();

    for (auto it = root.begin(); it != root.end(); ++it)
    {
        Action action = Action::Count;
        if (!TryParseAction(it.key().c_str(), action))
        {
            std::cerr << "Sapana InputBindings: unknown action '" << it.key() << "', skipping\n";
            continue;
        }

        if (!it.value().is_array())
        {
            std::cerr << "Sapana InputBindings: bindings for '" << it.key() << "' must be an array\n";
            continue;
        }

        BindingList list;
        for (const auto& entry : it.value())
        {
            if (entry.is_string())
                list.push_back(entry.get<std::string>());
        }
        m_Bindings[static_cast<std::size_t>(action)] = std::move(list);
    }

    return true;
}

const BindingList& InputBindings::GetBindings(Action action) const
{
    return m_Bindings[static_cast<std::size_t>(action)];
}

} // namespace input
} // namespace sapana
