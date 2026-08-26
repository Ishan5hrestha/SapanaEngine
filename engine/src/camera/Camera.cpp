#include "sapana/camera/Camera.hpp"

#include <algorithm>
#include <cmath>

namespace sapana
{
namespace camera
{

Camera::Camera()
{
    UpdateViewMatrix();
}

void Camera::SetPosition(const float3& pos)
{
    m_Position = pos;
    UpdateViewMatrix();
}

void Camera::SetRotation(float yaw, float pitch)
{
    m_Yaw   = yaw;
    m_Pitch = pitch;
    ClampPitch();
    UpdateViewMatrix();
}

void Camera::SetPerspective(float fovY, float nearPlane, float farPlane, bool isOpenGL)
{
    m_FovY     = fovY;
    m_Near     = nearPlane;
    m_Far      = farPlane;
    m_IsOpenGL = isOpenGL;
}

void Camera::ApplyConfig(const CameraConfig& config)
{
    m_MoveSpeed       = config.MoveSpeed;
    m_LookSensitivity = config.LookSensitivity;
    m_FovY            = config.FovYDegrees * (PI_F / 180.f);
    m_Near            = config.NearPlane;
    m_Far             = config.FarPlane;
    m_PitchLimit      = config.PitchLimitDegrees * (PI_F / 180.f);
    ClampPitch();
    UpdateViewMatrix();
}

void Camera::Move(const float3& direction, float deltaTime)
{
    float3 dir = direction;
    const float len = Diligent::length(dir);
    if (len == 0.0f)
        return;

    dir /= len;

    // Horizontal movement relative to yaw only (classic FPS).
    const float3 localHoriz{dir.x, 0.f, dir.z};
    const float3 worldHoriz = localHoriz * float4x4::RotationY(m_Yaw).Transpose();
    const float3 worldDelta{worldHoriz.x, dir.y, worldHoriz.z};

    m_Position += worldDelta * m_MoveSpeed * deltaTime;
    UpdateViewMatrix();
}

void Camera::Look(float deltaYaw, float deltaPitch)
{
    m_Yaw += deltaYaw;
    m_Pitch += deltaPitch;
    ClampPitch();
    UpdateViewMatrix();
}

void Camera::LookPixels(float deltaX, float deltaY)
{
    Look(-deltaX * m_LookSensitivity, -deltaY * m_LookSensitivity);
}

float4x4 Camera::GetViewMatrix() const
{
    return m_ViewMatrix;
}

float4x4 Camera::GetProjectionMatrix(float aspectRatio) const
{
    return float4x4::Projection(m_FovY, aspectRatio, m_Near, m_Far, m_IsOpenGL);
}

void Camera::ClampPitch()
{
    m_Pitch = std::clamp(m_Pitch, -m_PitchLimit, m_PitchLimit);
}

void Camera::UpdateViewMatrix()
{
    const float4x4 cameraRotation = float4x4::RotationY(m_Yaw) * float4x4::RotationX(m_Pitch);
    m_ViewMatrix                  = float4x4::Translation(-m_Position) * cameraRotation;
}

} // namespace camera
} // namespace sapana
