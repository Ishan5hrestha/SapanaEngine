#pragma once

#include "BasicMath.hpp"

#include <vector>

namespace sapana
{
namespace render
{

/// Maximum punctual/directional lights written into the PBR frame CB (matches MaxLightCount).
inline constexpr int kMaxConfiguredLights = 4;

enum class LightType
{
    Directional = 0
};

struct LightDesc
{
    LightType        Type      = LightType::Directional;
    Diligent::float3 Direction{-0.4f, -1.f, -0.3f}; // world-space; normalized on load
    Diligent::float3 Color{1.f, 1.f, 1.f};
    float            Intensity = 3.f;
};

/// Procedural gradient sky (view-ray). Parsed from lighting.json "sky" block.
struct SkyConfig
{
    bool             Enabled = true;
    Diligent::float3 ZenithColor{0.15f, 0.35f, 0.75f};
    Diligent::float3 HorizonColor{0.55f, 0.65f, 0.78f};
    Diligent::float3 GroundColor{0.22f, 0.20f, 0.18f};
    float            HorizonFalloff = 0.35f;
};

/// Cascaded / single-map PCF shadows for the primary directional light.
/// Note: Diligent PBR samples one WorldToLight matrix per light, so v1 uses 1 cascade.
struct ShadowConfig
{
    bool  Enabled     = true;
    int   Resolution  = 2048; // 512 / 1024 / 2048 / 4096
    int   Cascades    = 1;    // clamped 1..2; only cascade 0 is sampled by PBR (extra cascades waste GPU)
    int   PcfKernel   = 5;    // 2,3,5,7 — baked into PBR shaders at init
    float MaxDistance = 45.f; // tighter = more texels per meter
    float DepthBias   = 0.0015f;
};

/// Tunables for clear color, sun lights, sky, shadows, and tone mapping (config/lighting.json).
struct LightingConfig
{
    Diligent::float4 ClearColor{0.35f, 0.35f, 0.35f, 1.f};
    bool             EnableIbl = false;
    SkyConfig        Sky;
    ShadowConfig     Shadows;

    std::vector<LightDesc> Lights;

    float AverageLogLum = 0.3f;
    float MiddleGray    = 0.18f;
    float WhitePoint    = 3.f;

    LightingConfig();

    /// Ensures at least one directional light exists (never empty for PBR Draw).
    void EnsureDefaultLight();

    /// Load from JSON. On failure leaves *this unchanged and returns false.
    bool LoadFromFile(const char* path);
};

} // namespace render
} // namespace sapana
