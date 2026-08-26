cbuffer Constants
{
    float4x4 g_WorldViewProj;
    float4x4 g_World;
    float4x4 g_WorldToLightProj;
    float4   g_ColorTint;
};

struct VSInput
{
    float3 Pos   : ATTRIB0;
    float4 Color : ATTRIB1;
};

struct PSInput
{
    float4 Pos      : SV_POSITION;
    float4 Color    : COLOR0;
    float3 WorldPos : COLOR1;
};

void main(in  VSInput VSIn,
          out PSInput PSIn)
{
    // Keep all CB members live so HLSL DCE cannot shrink the UBO vs C++ layout.
    float4 WorldPos = mul(float4(VSIn.Pos, 1.0), g_World);
    PSIn.Pos      = mul(float4(VSIn.Pos, 1.0), g_WorldViewProj);
    PSIn.WorldPos = WorldPos.xyz;
    PSIn.Color    = VSIn.Color * g_ColorTint + g_WorldToLightProj[3] * 0.0;
}
