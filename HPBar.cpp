#include "main.h"
#include "renderer.h"
#include "HPBar.h"
#include "Stats.h"

void HPBar::Init(float X, float Y, float Width, float Height, GameObject* Target, const WCHAR* FillTextureName)
{
    m_Layer = 4;
    m_X = X;
    m_Y = Y;
    m_Width = Width;
    m_Height = Height;
    m_Target = Target;

    VERTEX_3D bgVertex[4];
    bgVertex[0].Position = XMFLOAT3(X, Y, 0.0f);
    bgVertex[0].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    bgVertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    bgVertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

    bgVertex[1].Position = XMFLOAT3(X + Width, Y, 0.0f);
    bgVertex[1].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    bgVertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    bgVertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

    bgVertex[2].Position = XMFLOAT3(X, Y + Height, 0.0f);
    bgVertex[2].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    bgVertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    bgVertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

    bgVertex[3].Position = XMFLOAT3(X + Width, Y + Height, 0.0f);
    bgVertex[3].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    bgVertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    bgVertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

    D3D11_BUFFER_DESC bgDesc{};
    bgDesc.Usage = D3D11_USAGE_DEFAULT;
    bgDesc.ByteWidth = sizeof(VERTEX_3D) * 4;
    bgDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bgDesc.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA bgData{};
    bgData.pSysMem = bgVertex;

    Renderer::GetDevice()->CreateBuffer(&bgDesc, &bgData, &m_BackgroundVertexBuffer);

    // fill quad is dynamic - remapped every frame based on the HP ratio
    D3D11_BUFFER_DESC fillDesc{};
    fillDesc.Usage = D3D11_USAGE_DYNAMIC;
    fillDesc.ByteWidth = sizeof(VERTEX_3D) * 4;
    fillDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    fillDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA fillData{};
    fillData.pSysMem = bgVertex; // initial data, overwritten every Draw()

    Renderer::GetDevice()->CreateBuffer(&fillDesc, &fillData, &m_FillVertexBuffer);

    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\unlitTextureVS.cso");
    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\unlitTexturePS.cso");

    TexMetadata bgMetadata;
    ScratchImage bgImage;
    LoadFromWICFile(L"asset\\texture\\UI_Bar\\bar_background.png", WIC_FLAGS_NONE, &bgMetadata, bgImage);
    CreateShaderResourceView(Renderer::GetDevice(), bgImage.GetImages(),
        bgImage.GetImageCount(), bgMetadata, &m_BackgroundTexture);
    assert(m_BackgroundTexture);

    TexMetadata fillMetadata;
    ScratchImage fillImage;
    LoadFromWICFile(FillTextureName, WIC_FLAGS_NONE, &fillMetadata, fillImage);
    CreateShaderResourceView(Renderer::GetDevice(), fillImage.GetImages(),
        fillImage.GetImageCount(), fillMetadata, &m_FillTexture);
    assert(m_FillTexture);
}

void HPBar::Uninit()
{
    m_BackgroundVertexBuffer->Release();
    m_FillVertexBuffer->Release();

    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();
}

void HPBar::Update()
{
}

void HPBar::Draw()
{
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    Renderer::SetWorldViewProjection2D();
    Renderer::SetWorldMatrix(XMMatrixIdentity());

    MATERIAL material{};
    material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
    material.TextureEnable = true;
    Renderer::SetMaterial(material);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // background
    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_BackgroundTexture);
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_BackgroundVertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->Draw(4, 0);

    // fill - width scaled by current HP ratio
    float ratio = 1.0f;
    if (m_Target != nullptr)
    {
        Stats* stats = m_Target->GetGameComponent<Stats>();
        if (stats != nullptr && stats->GetMaxHP() > 0)
            ratio = (float)stats->GetHP() / (float)stats->GetMaxHP();
    }
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    float fillWidth = m_Width * ratio;

    D3D11_MAPPED_SUBRESOURCE msr{};
    if (SUCCEEDED(Renderer::GetDeviceContext()->Map(m_FillVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
    {
        VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;

        vertex[0].Position = XMFLOAT3(m_X, m_Y, 0.0f);
        vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
        vertex[0].Diffuse = XMFLOAT4(1, 1, 1, 1);
        vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

        vertex[1].Position = XMFLOAT3(m_X + fillWidth, m_Y, 0.0f);
        vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
        vertex[1].Diffuse = XMFLOAT4(1, 1, 1, 1);
        vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

        vertex[2].Position = XMFLOAT3(m_X, m_Y + m_Height, 0.0f);
        vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
        vertex[2].Diffuse = XMFLOAT4(1, 1, 1, 1);
        vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

        vertex[3].Position = XMFLOAT3(m_X + fillWidth, m_Y + m_Height, 0.0f);
        vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
        vertex[3].Diffuse = XMFLOAT4(1, 1, 1, 1);
        vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

        Renderer::GetDeviceContext()->Unmap(m_FillVertexBuffer, 0);
    }

    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_FillTexture);
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_FillVertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->Draw(4, 0);
}