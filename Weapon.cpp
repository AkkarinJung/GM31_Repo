#include "main.h"
#include "Weapon.h"

void Weapon::Init()
{
    // Vector3's default constructor leaves its members alone, so the hit
    // point arrays start as whatever was on the heap. Nothing reads them
    // before RecordFrameHit fills them, but a stray NaN reaching a world
    // matrix is not a bug worth leaving open for the sake of one loop.
    for (int i = 0; i < FRAME_HIT_MAX; i++)
    {
        m_FrameHitPoint[i] = Vector3(0.0f, 0.0f, 0.0f);
        m_FrameHitDirection[i] = Vector3(1.0f, 0.0f, 0.0f);
    }

    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\litTextureVS.cso");
    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\litTexturePS.cso");
    LoadModel();
}

void Weapon::Uninit()
{
    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();

    GameObject::Uninit();
}

void Weapon::Update()
{
    if (m_CooldownTimer > 0.0f)
    {
        m_CooldownTimer -= 1.0f / 60.0f;
    }

    GameObject::Update();
}

void Weapon::Draw()
{
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    GameObject::Draw();
}
