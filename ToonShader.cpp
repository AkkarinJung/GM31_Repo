#include "main.h"
#include "renderer.h"
#include "ToonShader.h"

static ID3D11InputLayout*        s_VertexLayout = nullptr;
static ID3D11VertexShader*       s_VertexShader = nullptr;
static ID3D11PixelShader*        s_PixelShader = nullptr;
static ID3D11ShaderResourceView* s_RampTexture = nullptr;
static bool                      s_Loaded = false;

// x = ramp row, y = rim start, z = rim darkness, w = rim softness.
const XMFLOAT4 ToonShader::CharacterLook{ 0.125f, -0.35f, 0.30f, 0.15f };
const XMFLOAT4 ToonShader::SceneryLook{ 0.125f, -0.25f, 0.55f, 0.25f };

void ToonShader::LoadShared()
{
    if (s_Loaded)
        return;

    Renderer::CreateVertexShader(&s_VertexShader, &s_VertexLayout,
        "shader\\toonVS.cso");

    Renderer::CreatePixelShader(&s_PixelShader,
        "shader\\toonPS.cso");

    TexMetadata metadata;
    ScratchImage image;
    LoadFromWICFile(L"asset\\texture\\toon_ramp.png", WIC_FLAGS_NONE, &metadata, image);
    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &s_RampTexture);
    assert(s_RampTexture);

    s_Loaded = true;
}

void ToonShader::UninitShared()
{
    if (s_VertexLayout) { s_VertexLayout->Release(); s_VertexLayout = nullptr; }
    if (s_VertexShader) { s_VertexShader->Release(); s_VertexShader = nullptr; }
    if (s_PixelShader)  { s_PixelShader->Release();  s_PixelShader = nullptr; }
    if (s_RampTexture)  { s_RampTexture->Release();  s_RampTexture = nullptr; }

    s_Loaded = false;
}

void ToonShader::Bind(const XMFLOAT4& Parameter)
{
    LoadShared(); // no-op once loaded - covers anything that draws before Game::Init got to it

    Renderer::GetDeviceContext()->IASetInputLayout(s_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(s_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(s_PixelShader, NULL, 0);

    Renderer::SetParameter(Parameter);

    // t0 is the model's own texture, bound by whatever draws the mesh.
    // Only the ramp belongs to the shader.
    Renderer::GetDeviceContext()->PSSetShaderResources(1, 1, &s_RampTexture);
}
