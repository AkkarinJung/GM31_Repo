#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Prop.h"
#include "animationModel.h"

// Every model in the Enviroment pack carries an Lcl Scaling of 100 on its node,
// which AnimationModel bakes into the vertices - a 2 metre bench arrives 200
// units long. A hundredth puts the whole set into metres, and the player is
// 1.8 units tall, so metres are game units and one number does for all of them.
static const float PROP_SCALE = 0.01f;

void Prop::Init()
{
    m_Layer = 1; // with the terrain and the crates, not the effects

    m_Scale = { PROP_SCALE, PROP_SCALE, PROP_SCALE };

    // AnimationModel::Draw sets its own material and texture but not the
    // shaders, so this binds them itself - the same as Box and Hedge.
    // The toon shader already in the project - shader\toonVS.cso and
    // shader\toonPS.cso, the same pair Enemy loads.
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\toonVS.cso");

    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\toonPS.cso");

    TexMetadata metadata;
    ScratchImage image;
    LoadFromWICFile(L"asset\\texture\\toon_ramp.png", WIC_FLAGS_NONE, &metadata, image);
    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &m_RampTexture);
    assert(m_RampTexture);
}

void Prop::Load(const char* FileName, float SizeScale)
{
    // ModelRenderer only parses Wavefront OBJ, so FBX goes through
    // AnimationModel - the same route Box, Sword and Hedge take.
    AnimationModel* animationModel = AddGameComponent<AnimationModel>(this);
    animationModel->Load(FileName);

    m_Scale = { PROP_SCALE * SizeScale, PROP_SCALE * SizeScale, PROP_SCALE * SizeScale };
}

void Prop::Uninit()
{
    if (m_VertexLayout) { m_VertexLayout->Release(); m_VertexLayout = nullptr; }
    if (m_VertexShader) { m_VertexShader->Release(); m_VertexShader = nullptr; }
    if (m_PixelShader) { m_PixelShader->Release();  m_PixelShader = nullptr; }
    if (m_RampTexture) { m_RampTexture->Release(); m_RampTexture = nullptr; }

    GameObject::Uninit();
}

void Prop::Draw()
{
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    Renderer::SetParameter(m_Parameter);

    // t0 is the model's own texture, set by AnimationModel::Draw.
    // Only the ramp has to be bound here.
    Renderer::GetDeviceContext()->PSSetShaderResources(1, 1, &m_RampTexture);

    GameObject::Draw();
}
