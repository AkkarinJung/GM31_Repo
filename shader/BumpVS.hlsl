#include "common.hlsl"


void main(in VS_IN In, out BUMP_PS_IN Out)
{
    matrix wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    Out.Position = mul(In.Position, wvp);

    float4 normal = float4(In.Normal.xyz, 0.0f);
    normal = normalize(mul(normal, World));
    Out.Normal = normal;

    // The mesh has no tangent in its vertex format, so build one here.
    // UVs run along world X / Z, so take the world X axis and push it
    // perpendicular to the normal (Gram-Schmidt); the binormal follows.
    float3 worldX = normalize(mul(float4(1.0f, 0.0f, 0.0f, 0.0f), World).xyz);
    float3 T = normalize(worldX - normal.xyz * dot(worldX, normal.xyz));
    float3 B = cross(normal.xyz, T);

    Out.Tangent = float4(T, 0.0f);
    Out.Binormal = float4(B, 0.0f);

    Out.Diffuse = In.Diffuse * Material.Diffuse;
    Out.TexCoord = In.TexCoord;
    Out.WorldPosition = mul(In.Position, World);
}