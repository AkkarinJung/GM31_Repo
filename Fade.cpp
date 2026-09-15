#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Fade.h"

namespace
{
    // The same fixed step the rest of the game runs on.
    const float DELTA_TIME = 1.0f / 60.0f;
}

void Fade::Init()
{
    m_Layer = FADE_LAYER;

    // A screen sized quad in 2D space, which is what SetWorldViewProjection2D
    // below expects: pixels, origin top left.
    const float x = 0.0f;
    const float y = 0.0f;
    const float width = SCREEN_WIDTH;
    const float height = SCREEN_HEIGHT;

    VERTEX_3D vertex[4];

    vertex[0].Position = XMFLOAT3(x, y, 0.0f);
    vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

    vertex[1].Position = XMFLOAT3(x + width, y, 0.0f);
    vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

    vertex[2].Position = XMFLOAT3(x, y + height, 0.0f);
    vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

    vertex[3].Position = XMFLOAT3(x + width, y + height, 0.0f);
    vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(VERTEX_3D) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertex;

    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

    // The quad is a flat colour - the texture is switched off in Draw - but
    // this is the shader pair every other 2D object here uses, so the fade
    // goes through the same path as the UI it covers.
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\unlitTextureVS.cso");

    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\unlitTexturePS.cso");
}

void Fade::Uninit()
{
    // Guarded, and the members are initialised to nullptr in the header.
    // A scene change deletes every object, and an object built but never
    // fully set up would otherwise release whatever happened to be on the
    // stack.
    if (m_VertexBuffer != nullptr) { m_VertexBuffer->Release(); m_VertexBuffer = nullptr; }
    if (m_VertexLayout != nullptr) { m_VertexLayout->Release(); m_VertexLayout = nullptr; }
    if (m_VertexShader != nullptr) { m_VertexShader->Release(); m_VertexShader = nullptr; }
    if (m_PixelShader != nullptr) { m_PixelShader->Release(); m_PixelShader = nullptr; }

    GameObject::Uninit();
}

void Fade::Update()
{
    m_Time += DELTA_TIME;

    // A fade-IN is finished when it is clear, and gets out of the way. A
    // fade-OUT is finished when it is black and STAYS there: the scene change
    // it was paired with has not fired yet, and tidying itself away would
    // show the outgoing scene again for those last frames.
    if (!m_FadeOut && m_Time >= m_StartDelay + m_Duration)
        SetDestory();
}

float Fade::Alpha() const
{
    if (m_Duration <= 0.0f)
        return m_FadeOut ? 1.0f : 0.0f;

    // Still waiting to start - see OutBefore, which uses the delay to line the
    // black up with a scene change scheduled further out.
    if (m_Time < m_StartDelay)
        return m_FadeOut ? 0.0f : 1.0f;

    float t = (m_Time - m_StartDelay) / m_Duration;

    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    return m_FadeOut ? t : (1.0f - t);
}

void Fade::Draw()
{
    float alpha = Alpha();

    // Nothing to cover. Skipping the draw entirely keeps a finished fade-out
    // from paying for a full screen quad every frame while it waits for the
    // scene to swap - and keeps a fade-in that has already cleared invisible
    // on the frame before the reaper collects it.
    if (alpha <= 0.0f)
        return;

    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    Renderer::SetWorldViewProjection2D();

    XMMATRIX world, scale, rot, trans;
    scale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
    rot = XMMatrixRotationRollPitchYaw(0.0f, 0.0f, 0.0f);
    trans = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
    world = scale * rot * trans;

    Renderer::SetWorldMatrix(world);

    MATERIAL material{};
    material.Diffuse = { 0.0f, 0.0f, 0.0f, alpha };
    material.TextureEnable = false;
    Renderer::SetMaterial(material);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    Renderer::GetDeviceContext()->Draw(4, 0);
}

Fade* Fade::In(float Duration)
{
    Fade* fade = Manager::AddGameObj<Fade>();
    fade->m_FadeOut = false;
    fade->m_Duration = Duration;
    fade->m_StartDelay = 0.0f;
    fade->m_Time = 0.0f;

    return fade;
}

Fade* Fade::Out(float Duration, float StartDelay)
{
    Fade* fade = Manager::AddGameObj<Fade>();
    fade->m_FadeOut = true;
    fade->m_Duration = Duration;
    fade->m_StartDelay = (StartDelay > 0.0f) ? StartDelay : 0.0f;
    fade->m_Time = 0.0f;

    return fade;
}

Fade* Fade::OutBefore(float ChangeDelay, float Duration)
{
    // Hold clear for the slack, then darken into the swap. A change scheduled
    // sooner than the fade is long just fades for whatever time there is,
    // rather than being cut off part way to black.
    if (Duration > ChangeDelay)
        Duration = ChangeDelay;

    return Out(Duration, ChangeDelay - Duration);
}
