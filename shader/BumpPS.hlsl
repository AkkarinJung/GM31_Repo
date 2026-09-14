#include "common.hlsl"


// Ground shading, toon style. The light term is quantised through a ramp
// texture instead of falling off smoothly - the same ramp and the same light
// levels the enemies use in toonPS, so the whole scene reads as one art style.

// The normal maps are OpenGL-convention (green points up), DirectX expects
// the opposite. Uncomment if a map's bumps light from the wrong side.
//#define NORMALMAP_FLIP_GREEN


Texture2D g_Texture : register(t0); // albedo
Texture2D g_TextureNormal : register(t1); // normal
Texture2D g_TextureHeight : register(t2); // height (unused here, kept bound)
Texture2D g_TextureRoughness : register(t3); // roughness (unused here)
Texture2D g_TextureAO : register(t4); // ambient occlusion
Texture2D g_TextureMetallic : register(t5); // metallic (unused here)
Texture2D g_TextureRamp : register(t6); // toon ramp, the enemies' texture
SamplerState g_SamplerState : register(s0);


// The scene light is a hot 1.5 diffuse / 0.5 ambient, which blows the ramp
// out to white. toonPS uses these levels instead; matching them keeps the
// ground and the enemies lit the same way.
static const float3 ToonLightColor = float3(0.9f, 0.9f, 0.9f);
static const float3 ToonAmbientColor = float3(0.3f, 0.3f, 0.3f);


// Parameter comes from Renderer::SetParameter, set in MeshField::Draw:
//   x = which row of the ramp to read   y = AO strength
//   z = macro variation amount          w = normal map strength


// How far the ground drifts between the two tints below. 0.05 puts a patch
// every 20 world units - about three tiles, so it breaks the repeat up
// without looking like blotches.
static const float MacroScale = 0.05f;
static const float3 MacroTintA = float3(0.82f, 0.95f, 0.72f);
static const float3 MacroTintB = float3(1.06f, 1.02f, 0.80f);


// Value noise off the world position. The texture repeats every few metres
// and the eye picks that up immediately; drifting the tint over a much larger
// distance hides it. Cheap enough to be worth not carrying a second texture.
float Hash(float2 p)
{
    return frac(sin(dot(p, float2(127.1f, 311.7f))) * 43758.5453f);
}

float ValueNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);
    f = f * f * (3.0f - 2.0f * f);

    return lerp(lerp(Hash(i), Hash(i + float2(1.0f, 0.0f)), f.x),
				lerp(Hash(i + float2(0.0f, 1.0f)), Hash(i + float2(1.0f, 1.0f)), f.x), f.y);
}


void main(in BUMP_PS_IN In, out float4 outDiffuse : SV_Target)
{
    float rampRow = Parameter.x;
    float aoStrength = Parameter.y;
    float macroAmount = Parameter.z;
    float normalStrength = Parameter.w;

    // Normal map off at strength 0: flattening xy leaves (0,0,1), which comes
    // back out of the TBN as the plain surface normal. The texture is painted
    // with its own light and shade, and a second set of bumps on top fights
    // it - raise w only if you want the surface to catch the light as well.
    float3 tangentNormal = (g_TextureNormal.Sample(g_SamplerState, In.TexCoord).rgb * 2.0f) - 1.0f;
#ifdef NORMALMAP_FLIP_GREEN
    tangentNormal.y = -tangentNormal.y;
#endif
    tangentNormal.xy *= normalStrength;
    tangentNormal = normalize(tangentNormal);

    float3x3 tbn = float3x3(normalize(In.Tangent.xyz),
							normalize(In.Binormal.xyz),
							normalize(In.Normal.xyz));
    float3 normal = normalize(mul(tangentNormal, tbn));

    float3 lv = normalize(-Light.Direction.xyz);

    // Half lambert, so the ramp gets the full 0..1 range to work with rather
    // than only the lit half. Same as toonPS.
    float light = 0.5f + 0.5f * dot(normal, lv);
    light = clamp(light, 0.01f, 0.99f);

    // The ramp holds several looks stacked as rows; x picks which one.
    float row = clamp(rampRow, 0.01f, 0.99f);
    float3 toon = g_TextureRamp.Sample(g_SamplerState, float2(light, row)).rgb;

    float ao = lerp(1.0f, g_TextureAO.Sample(g_SamplerState, In.TexCoord).r, aoStrength);

    outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
    outDiffuse.rgb *= toon * In.Diffuse.rgb * ToonLightColor + ToonAmbientColor * ao;
    outDiffuse.a *= In.Diffuse.a;

    // Two octaves of drift across the field, so it stops reading as one
    // uniform green with a tile pattern in it.
    float2 worldXZ = In.WorldPosition.xz;
    float macro = ValueNoise(worldXZ * MacroScale) * 0.65f
				+ ValueNoise(worldXZ * MacroScale * 2.7f) * 0.35f;

    float3 tint = lerp(MacroTintA, MacroTintB, macro);
    float shade = lerp(0.80f, 1.15f, macro);

    outDiffuse.rgb *= lerp(1.0f, tint * shade, macroAmount);
}
