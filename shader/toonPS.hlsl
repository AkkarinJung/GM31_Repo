#include "common.hlsl"


Texture2D g_Texture : register(t0);
Texture2D g_TextureRamp : register(t1);
SamplerState g_SamplerState : register(s0);


// The renderer's scene light is a hot 1.5 diffuse / 0.5 ambient, which blows
// the ramp out. These are the class sample's levels instead. Swap them for
// Light.Diffuse.rgb / Light.Ambient.rgb below to use the scene light directly.
static const float3 ToonLightColor = float3(0.9f, 0.9f, 0.9f);
static const float3 ToonAmbientColor = float3(0.3f, 0.3f, 0.3f);


void main(in TOON_PS_IN In, out float4 outDiffuse : SV_TARGET)
{
    // Enemy.cpp's hurt / attack flash arrives as a flat untextured material
    // (see ModelRenderer::Draw). Keep it flat and unlit so the hit feedback
    // still reads instead of being shaded and edge darkened.
    if (!Material.TextureEnable)
    {
        outDiffuse = In.Diffuse;
        return;
    }
    
    // Vector towards the light. The scene light is directional, so this is
    // just the direction flipped - no Light.Position / PointLightParam in this
    // engine's LIGHT, so no distance falloff (ofs) either.
    float4 lv = -Light.Direction;
    lv = normalize(lv);
    
    float4 normal = normalize(In.Normal);
    
    // half lambert, so the ramp gets the full 0..1 range to work with
    float light = 0.5f + 0.5f * dot(normal.xyz, lv.xyz);
    light = clamp(light, 0.01f, 0.99f);
    
    // Parameter.x picks the row of the ramp, so one texture holds several
    // looks (set from Enemy::Draw through Renderer::SetParameter)
    float texv = Parameter.x;
    texv = clamp(texv, 0.01f, 0.99f);
    
    float4 toon = g_TextureRamp.Sample(g_SamplerState, float2(light, texv));
    
    outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
    outDiffuse.rgb *= toon.rgb * In.Diffuse.rgb * ToonLightColor + ToonAmbientColor;
    //outDiffuse.rgb *= toon.rgb * In.Diffuse.rgb * Light.Diffuse.rgb + Light.Ambient.rgb;
    outDiffuse.a *= In.Diffuse.a;
    
    // Create view vector
    float3 eyev = normalize(In.WorldPosition.xyz - CameraPosition.xyz);
    
    // Dot product of view vector and normal
    float d = dot(eyev, normal.xyz);
    
    // Parameter.y = where the edge starts (more negative = thicker)
    // Parameter.w = how soft the transition is
    // Parameter.z = how dark the edge goes
    float edge = smoothstep(Parameter.y - Parameter.w, Parameter.y, d);
    outDiffuse.rgb *= lerp(1.0f, Parameter.z, edge);
    
}
