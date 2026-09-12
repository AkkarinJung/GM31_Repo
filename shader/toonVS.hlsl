#include "common.hlsl"

void main(in VS_IN In, out TOON_PS_IN Out)
{
    matrix wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    Out.Position = mul(In.Position, wvp);
    
    float4 worldNormal, normal;
    normal = float4(In.Normal.xyz, 0.0f);
    worldNormal = mul(normal, World);
    worldNormal = normalize(worldNormal);
    Out.Normal = worldNormal;
    
    // Material.Diffuse is how ModelRenderer::Draw pushes Enemy's hurt / attack
    // flash colour through, so it has to be folded in here
    Out.Diffuse = In.Diffuse * Material.Diffuse;
    Out.TexCoord = In.TexCoord;
    
    Out.WorldPosition = mul(In.Position, World);
}

