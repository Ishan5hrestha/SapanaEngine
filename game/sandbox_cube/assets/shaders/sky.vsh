cbuffer Constants
{
    float4x4 g_InvViewProj;
    float4   g_Zenith;   // rgb + unused
    float4   g_Horizon;
    float4   g_Ground;
    // std140: keep 16-byte alignment (float+float3 at offset 116 is illegal on Vulkan/SPIR-V).
    float4   g_FalloffPad; // x = horizon falloff
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float3 Ray : RAY;
};

void main(in uint Vid : SV_VertexID, out PSInput PSIn)
{
    // Fullscreen triangle covering clip space.
    float2 uv  = float2((Vid << 1u) & 2u, Vid & 2u);
    float4 clip = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 1.0, 1.0);
    PSIn.Pos = clip;

    // Unproject far-plane clip to world (view has translation stripped on CPU).
    float4 world = mul(clip, g_InvViewProj);
    PSIn.Ray = world.xyz / max(world.w, 1e-5);
}
