#include "main.h"
#include "Weapon.h"

void Weapon::Init()
{
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
