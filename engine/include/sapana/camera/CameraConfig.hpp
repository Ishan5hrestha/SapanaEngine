#pragma once

namespace sapana
{
namespace camera
{

/// Tunable first-person camera feel / projection (loaded from camera.json).
struct CameraConfig
{
    float MoveSpeed           = 4.0f;
    float LookSensitivity     = 0.003f; // radians per pixel
    float FovYDegrees         = 45.0f;
    float NearPlane           = 0.1f;
    float FarPlane            = 100.0f;
    float PitchLimitDegrees   = 89.0f;

    /// Load from JSON file. On failure, leaves *this unchanged and returns false.
    bool LoadFromFile(const char* path);
};

} // namespace camera
} // namespace sapana
