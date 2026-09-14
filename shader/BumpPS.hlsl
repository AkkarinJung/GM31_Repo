#include "common.hlsl"


// The grass maps are OpenGL-convention (green points up), DirectX expects
// the opposite. Comment this out if a map is already DirectX-convention.
//#define NORMALMAP_FLIP_GREEN

// Parallax quality. More steps = deeper looking bumps, more texture reads.
#define PARALLAX_MIN_STEPS	8
#define PARALLAX_MAX_STEPS	32


Texture2D g_Texture : register(t0); // albedo
Texture2D g_TextureNormal : register(t1); // normal
Texture2D g_TextureHeight : register(t2); // height
Texture2D g_TextureRoughness : register(t3); // roughness
Texture2D g_TextureAO : register(t4); // ambient occlusion
Texture2D g_TextureMetallic : register(t5); // metallic
SamplerState g_SamplerState : register(s0);


// Parameter comes from Renderer::SetParameter, set in MeshField::Draw:
//   x = parallax depth   y = AO strength
//   z = specular amount  w = normal map strength


// Walk into the height map along the view ray until we hit the surface,
// so the bumps slide against each other instead of staying flat.
float2 ParallaxOcclusion(float2 TexCoord, float3 ViewT, float Depth)
{
    if (Depth <= 0.0f)
        return TexCoord;

    // Head-on needs few steps, grazing angles need many.
    float steps = lerp((float) PARALLAX_MAX_STEPS, (float) PARALLAX_MIN_STEPS, saturate(ViewT.z));
    float layerStep = 1.0f / steps;

    // How far the UV slides per layer. The max() keeps the offset sane when
    // looking almost edge-on, where ViewT.z goes to zero.
    float2 uvStep = (ViewT.xy / max(ViewT.z, 0.1f)) * Depth * layerStep;

    // Grab the mip gradients out here: the loop below ends at a different
    // step per pixel, and Sample() can't work out its own gradients inside
    // branching flow, so every lookup in the loop uses SampleGrad.
    float2 ddxUV = ddx(TexCoord);
    float2 ddyUV = ddy(TexCoord);

    float2 uv = TexCoord;
    float currentLayer = 0.0f;
    // Height maps store white as high, so depth is the other way round.
    float currentDepth = 1.0f - g_TextureHeight.SampleGrad(g_SamplerState, uv, ddxUV, ddyUV).r;

    int taken = 0;

    [loop]
    for (int i = 0; i < PARALLAX_MAX_STEPS; i++)
    {
        if (currentLayer >= currentDepth || i >= (int) steps)
            break;

        uv -= uvStep;
        currentDepth = 1.0f - g_TextureHeight.SampleGrad(g_SamplerState, uv, ddxUV, ddyUV).r;
        currentLayer += layerStep;
        taken++;
    }

    // Already at the surface on the first sample - nothing to offset.
    if (taken == 0)
        return TexCoord;

    // Blend between the step that went too far and the one before it, so
    // the steps don't show up as bands.
    float2 prevUV = uv + uvStep;
    float after = currentDepth - currentLayer;
    float before = (1.0f - g_TextureHeight.SampleGrad(g_SamplerState, prevUV, ddxUV, ddyUV).r)
					- (currentLayer - layerStep);

    float denom = after - before;
    float weight = abs(denom) > 0.00001f ? after / denom : 0.0f;
    return lerp(uv, prevUV, saturate(weight));
}


void main(in BUMP_PS_IN In, out float4 outDiffuse : SV_Target)
{
    float parallaxDepth = Parameter.x;
    float aoStrength = Parameter.y;
    float specAmount = Parameter.z;
    float normalStrength = Parameter.w;

    // TBN rows are the basis vectors: mul(vector, tbn) goes tangent -> world,
    // mul(tbn, vector) goes world -> tangent.
    float3 N = normalize(In.Normal.xyz);
    float3 T = normalize(In.Tangent.xyz);
    float3 B = normalize(In.Binormal.xyz);
    float3x3 tbn = float3x3(T, B, N);

    float3 eyev = normalize(In.WorldPosition.xyz - CameraPosition.xyz);
    float3 viewT = mul(tbn, -eyev);

    // Offset every other lookup by the parallax result.
    float2 texCoord = ParallaxOcclusion(In.TexCoord, viewT, parallaxDepth);

    // Normal map -> world space.
    float3 tangentNormal = (g_TextureNormal.Sample(g_SamplerState, texCoord).rgb * 2.0f) - 1.0f;
#ifdef NORMALMAP_FLIP_GREEN
    tangentNormal.y = -tangentNormal.y;
#endif
    tangentNormal.xy *= normalStrength;
    tangentNormal = normalize(tangentNormal);

    float3 normal = normalize(mul(tangentNormal, tbn));

    float4 albedo = g_Texture.Sample(g_SamplerState, texCoord);
    float roughness = g_TextureRoughness.Sample(g_SamplerState, texCoord).r;
    float metallic = g_TextureMetallic.Sample(g_SamplerState, texCoord).r;
    float ao = g_TextureAO.Sample(g_SamplerState, texCoord).r;

    // AO only darkens the ambient, and only as far as the strength allows.
    ao = lerp(1.0f, ao, aoStrength);

    // Metal has no diffuse and tints its highlight with its own colour;
    // everything else keeps its diffuse and uses a plain grey highlight.
    float3 diffuseColor = albedo.rgb * (1.0f - metallic);
    float3 specColor = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, metallic);

    float3 lightDir = normalize(Light.Direction.xyz);
    float light = saturate(-dot(normal, lightDir));

    outDiffuse.rgb = diffuseColor * In.Diffuse.rgb
					* (Light.Diffuse.rgb * light + Light.Ambient.rgb * ao);
    outDiffuse.a = albedo.a * In.Diffuse.a;

    // Rough surfaces get a wide, weak highlight, smooth ones a tight, strong
    // one. 2 .. 512 covers dull grass through to wet stone.
    float smoothness = 1.0f - roughness;
    float specPower = exp2(1.0f + smoothness * 9.0f);

    float3 halfv = normalize(eyev + lightDir);
    float specular = pow(saturate(-dot(normal, halfv)), specPower);

    outDiffuse.rgb += specColor * specular * specAmount * smoothness
					* Light.Diffuse.rgb * ao;
}