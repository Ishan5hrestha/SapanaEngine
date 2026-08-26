#include "sapana/input/InputSystem.hpp"

#include <cmath>
#include <cstring>

#include "imgui.h"

namespace sapana
{
namespace input
{

namespace
{
constexpr float kLookDeadzonePixels = 0.5f;
} // namespace

bool InputSystem::LoadBindings(const char* path)
{
    return m_Bindings.LoadFromFile(path);
}

void InputSystem::NotifyPointerWarped()
{
    m_AbsorbNextMouseSample = true;
}

void InputSystem::SuppressLookFrames(unsigned int frameCount)
{
    m_SuppressLookFrames = frameCount;
}

Diligent::InputKeys InputSystem::DiligentKeyFromToken(const std::string& token) const
{
    if (token == "Diligent.MoveForward")
        return Diligent::InputKeys::MoveForward;
    if (token == "Diligent.MoveBackward")
        return Diligent::InputKeys::MoveBackward;
    if (token == "Diligent.MoveLeft")
        return Diligent::InputKeys::MoveLeft;
    if (token == "Diligent.MoveRight")
        return Diligent::InputKeys::MoveRight;
    if (token == "Diligent.MoveUp")
        return Diligent::InputKeys::MoveUp;
    if (token == "Diligent.MoveDown")
        return Diligent::InputKeys::MoveDown;
    if (token == "Diligent.Reset")
        return Diligent::InputKeys::Reset;
    if (token == "Diligent.ShiftDown")
        return Diligent::InputKeys::ShiftDown;
    if (token == "Diligent.ControlDown")
        return Diligent::InputKeys::ControlDown;
    if (token == "Diligent.AltDown")
        return Diligent::InputKeys::AltDown;
    return Diligent::InputKeys::Unknown;
}

bool InputSystem::EvaluateKeyToken(const std::string& token, ImGuiIO* io) const
{
    if (io == nullptr)
        return false;

    if (token.size() == 5 && token.compare(0, 4, "Key.") == 0)
    {
        const char letter = token[4];
        if (letter >= 'A' && letter <= 'Z')
        {
            const ImGuiKey key = static_cast<ImGuiKey>(ImGuiKey_A + (letter - 'A'));
            return ImGui::IsKeyPressed(key, false);
        }
        if (letter >= 'a' && letter <= 'z')
        {
            const ImGuiKey key = static_cast<ImGuiKey>(ImGuiKey_A + (letter - 'a'));
            return ImGui::IsKeyPressed(key, false);
        }
    }
    return false;
}

bool InputSystem::EvaluateDigital(const BindingList& bindings, Diligent::InputController& controller, ImGuiIO* io) const
{
    for (const std::string& token : bindings)
    {
        if (token.compare(0, 9, "Diligent.") == 0)
        {
            const Diligent::InputKeys key = DiligentKeyFromToken(token);
            if (key != Diligent::InputKeys::Unknown && controller.IsKeyDown(key))
                return true;
        }
        else if (token.compare(0, 4, "Key.") == 0)
        {
            if (EvaluateKeyToken(token, io))
                return true;
        }
    }
    return false;
}

void InputSystem::Update(Diligent::InputController& controller, ImGuiIO* io)
{
    m_LookDelta = float2{0.f, 0.f};

    for (std::size_t i = 0; i < static_cast<std::size_t>(Action::Count); ++i)
    {
        const Action       action   = static_cast<Action>(i);
        const BindingList& bindings = m_Bindings.GetBindings(action);

        if (action == Action::Look)
        {
            bool wantsMouseDelta = false;
            for (const std::string& token : bindings)
            {
                if (token == "Mouse.Delta")
                    wantsMouseDelta = true;
            }

            Diligent::MouseState mouse = controller.GetMouseState();

            if (m_SuppressLookFrames > 0)
            {
                --m_SuppressLookFrames;
                m_LastMouseState         = mouse;
                m_AbsorbNextMouseSample  = false;
                m_Down[i]                = false;
                m_WasPressed[i]          = false;
                continue;
            }

            // After a warp, adopt the real OS landing position as baseline — do not look.
            // (Theoretical Width/2,Height/2 often disagrees with where X actually places the cursor.)
            if (m_AbsorbNextMouseSample)
            {
                m_AbsorbNextMouseSample = false;
                m_LastMouseState        = mouse;
                m_Down[i]               = false;
                m_WasPressed[i]         = false;
                continue;
            }

            if (wantsMouseDelta && m_LastMouseState.IsValid() && mouse.IsValid())
            {
                m_LookDelta.x = mouse.PosX - m_LastMouseState.PosX;
                m_LookDelta.y = mouse.PosY - m_LastMouseState.PosY;

                if (std::fabs(m_LookDelta.x) < kLookDeadzonePixels)
                    m_LookDelta.x = 0.f;
                if (std::fabs(m_LookDelta.y) < kLookDeadzonePixels)
                    m_LookDelta.y = 0.f;
            }

            m_LastMouseState = mouse;
            m_Down[i]        = (m_LookDelta.x != 0.f || m_LookDelta.y != 0.f);
            m_WasPressed[i]  = false;
            continue;
        }

        if (action == Action::ToggleCursor)
        {
            const bool pressed = EvaluateDigital(bindings, controller, io);
            m_WasPressed[i]    = pressed;
            m_Down[i]          = pressed;
            continue;
        }

        const bool down = EvaluateDigital(bindings, controller, io);
        m_WasPressed[i] = down && !m_Down[i];
        m_Down[i]       = down;
    }
}

bool InputSystem::IsDown(Action action) const
{
    return m_Down[static_cast<std::size_t>(action)];
}

bool InputSystem::WasPressed(Action action) const
{
    return m_WasPressed[static_cast<std::size_t>(action)];
}

} // namespace input
} // namespace sapana
