#pragma once

#include "BasicMath.hpp"
#include "InputController.hpp"
#include "sapana/input/Action.hpp"
#include "sapana/input/InputBindings.hpp"

#include <array>
#include <string>

struct ImGuiIO;

namespace sapana
{
namespace input
{

using Diligent::float2;

/// Polls Diligent InputController (+ ImGui for raw Key.M) into action state.
/// Does not talk to Camera or CursorController.
class InputSystem
{
public:
    bool LoadBindings(const char* path);

    /// Update action state for this frame. Look uses frame-to-frame mouse delta.
    void Update(Diligent::InputController& controller, ImGuiIO* io);

    /// Call after XWarpPointer (or similar). The next Update absorbs the landing
    /// position as the new baseline without applying look — prevents drift when
    /// the OS rest position differs from the theoretical window center.
    void NotifyPointerWarped();

    /// Skip applying Look for the next N updates (e.g. after enabling capture).
    void SuppressLookFrames(unsigned int frameCount);

    bool IsDown(Action action) const;
    bool WasPressed(Action action) const;

    float2 GetLookDelta() const { return m_LookDelta; }

private:
    bool EvaluateDigital(const BindingList& bindings, Diligent::InputController& controller, ImGuiIO* io) const;
    bool EvaluateKeyToken(const std::string& token, ImGuiIO* io) const;
    Diligent::InputKeys DiligentKeyFromToken(const std::string& token) const;

    InputBindings m_Bindings;
    std::array<bool, static_cast<std::size_t>(Action::Count)> m_Down{};
    std::array<bool, static_cast<std::size_t>(Action::Count)> m_WasPressed{};
    Diligent::MouseState m_LastMouseState{};
    float2               m_LookDelta{0.f, 0.f};
    unsigned int         m_SuppressLookFrames  = 0;
    bool                 m_AbsorbNextMouseSample = false;
};

} // namespace input
} // namespace sapana
