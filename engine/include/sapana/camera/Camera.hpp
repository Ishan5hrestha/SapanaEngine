#pragma once

#include "BasicMath.hpp"
#include "sapana/camera/CameraConfig.hpp"

namespace sapana
{
namespace camera
{

using Diligent::float3;
using Diligent::float4x4;
using Diligent::PI_F;

/// First-person camera: position + yaw/pitch. Input-agnostic; the game layer
/// feeds Move / LookPixels deltas each frame.
class Camera
{
public:
    Camera();

    void SetPosition(const float3& pos);
    void SetRotation(float yaw, float pitch); // radians

    void SetPerspective(float fovY, float nearPlane, float farPlane, bool isOpenGL = false);
    void ApplyConfig(const CameraConfig& config);

    void SetMoveSpeed(float speed) { m_MoveSpeed = speed; }
    void SetLookSensitivity(float radiansPerPixel) { m_LookSensitivity = radiansPerPixel; }
    void SetPitchLimitRadians(float limit) { m_PitchLimit = limit; }

    /// Move in camera-local space (x = right, y = up, z = forward).
    /// Horizontal axes are yaw-relative; y is world-up. Scaled by move speed and deltaTime.
    void Move(const float3& direction, float deltaTime);

    /// Apply look deltas in radians. Pitch is clamped to avoid gimbal lock.
    void Look(float deltaYaw, float deltaPitch);

    /// Apply look from mouse pixel deltas using configured sensitivity.
    void LookPixels(float deltaX, float deltaY);

    float4x4 GetViewMatrix() const;
    float4x4 GetProjectionMatrix(float aspectRatio) const;

    float3 GetPosition() const { return m_Position; }
    float  GetYaw() const { return m_Yaw; }
    float  GetPitch() const { return m_Pitch; }

private:
    void UpdateViewMatrix();
    void ClampPitch();

    float3   m_Position{0.f, 0.f, -5.f};
    float    m_Yaw   = 0.f;
    float    m_Pitch = 0.f;
    float    m_FovY  = PI_F / 4.f;
    float    m_Near  = 0.1f;
    float    m_Far   = 100.f;
    bool     m_IsOpenGL         = false;
    float    m_MoveSpeed        = 4.f;
    float    m_LookSensitivity  = 0.003f;
    float    m_PitchLimit       = PI_F / 2.f - 0.01f;
    float4x4 m_ViewMatrix;
};

} // namespace camera
} // namespace sapana
