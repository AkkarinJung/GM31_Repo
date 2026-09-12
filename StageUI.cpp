#include "main.h"
#include "renderer.h"
#include "StageUI.h"
#include "manager.h"
#include "Enemy.h"
#include "Font.h"
#include "Game.h"
#include "Stage.h"

// The banner on the card sheet, reused here so every menu matches.
static const float BANNER_X = 64.0f, BANNER_Y = 827.0f, BANNER_W = 800.0f, BANNER_H = 77.0f;
static const float SHEET_WIDTH = 1536.0f;
static const float SHEET_HEIGHT = 1024.0f;

static const float NAME_HOLD = 2.5f; // seconds the stage name stays up
static const float NAME_FADE = 0.8f; // and how long it takes to fade out

void StageUI::Init()
{
    m_Layer = 4; // same UI layer as Score/HPBar/RoguelikeUI

    VERTEX_3D vertex[4]{};

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(VERTEX_3D) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertex; // initial data, overwritten every Draw()

    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\unlitTextureVS.cso");
    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\unlitTexturePS.cso");

    TexMetadata metadata;
    ScratchImage image;
    LoadFromWICFile(L"asset\\texture\\Simple_card_Design.png", WIC_FLAGS_NONE, &metadata, image);
    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &m_Texture);
    assert(m_Texture);
}

void StageUI::Uninit()
{
    m_VertexBuffer->Release();
    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();
    m_Texture->Release();
}

void StageUI::Update()
{
    // Paused objects do not update, so the hold time does not run down
    // behind the start of stage reward pick.
    m_Timer += 1.0f / 60.0f;

    GameObject::Update();
}

void StageUI::DrawBanner(float X, float Y, float Width, float Height, const XMFLOAT4& Color)
{
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    Renderer::SetWorldViewProjection2D();
    Renderer::SetWorldMatrix(XMMatrixIdentity());

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    MATERIAL material{};
    material.Diffuse = Color;
    material.TextureEnable = true;
    Renderer::SetMaterial(material);

    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);

    float u = BANNER_X / SHEET_WIDTH;
    float v = BANNER_Y / SHEET_HEIGHT;
    float uWidth = BANNER_W / SHEET_WIDTH;
    float vHeight = BANNER_H / SHEET_HEIGHT;

    D3D11_MAPPED_SUBRESOURCE msr{};
    if (SUCCEEDED(Renderer::GetDeviceContext()->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
    {
        VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;

        vertex[0].Position = XMFLOAT3(X, Y, 0.0f);
        vertex[0].Normal = XMFLOAT3(0, 0, 0);
        vertex[0].Diffuse = XMFLOAT4(1, 1, 1, 1);
        vertex[0].TexCoord = XMFLOAT2(u, v);

        vertex[1].Position = XMFLOAT3(X + Width, Y, 0.0f);
        vertex[1].Normal = XMFLOAT3(0, 0, 0);
        vertex[1].Diffuse = XMFLOAT4(1, 1, 1, 1);
        vertex[1].TexCoord = XMFLOAT2(u + uWidth, v);

        vertex[2].Position = XMFLOAT3(X, Y + Height, 0.0f);
        vertex[2].Normal = XMFLOAT3(0, 0, 0);
        vertex[2].Diffuse = XMFLOAT4(1, 1, 1, 1);
        vertex[2].TexCoord = XMFLOAT2(u, v + vHeight);

        vertex[3].Position = XMFLOAT3(X + Width, Y + Height, 0.0f);
        vertex[3].Normal = XMFLOAT3(0, 0, 0);
        vertex[3].Diffuse = XMFLOAT4(1, 1, 1, 1);
        vertex[3].TexCoord = XMFLOAT2(u + uWidth, v + vHeight);

        Renderer::GetDeviceContext()->Unmap(m_VertexBuffer, 0);
    }

    Renderer::GetDeviceContext()->Draw(4, 0);
}

void StageUI::Draw()
{
    int stage = Game::GetStageIndex();

    // No enemies left means Game is already changing scene - say so while
    // that plays out.
    if (Manager::GetGameObjs<Enemy>().size() == 0)
    {
        Font::DrawCentered(Game::IsRunComplete() ? "ALL STAGES CLEAR" : "STAGE CLEAR",
            SCREEN_WIDTH * 0.5f, SCREEN_HEIGHT * 0.5f - 40.0f, 52.0f,
            XMFLOAT4(1.0f, 0.95f, 0.6f, 1.0f));
        return;
    }

    if (m_Timer > NAME_HOLD + NAME_FADE)
        return;

    float alpha = 1.0f;
    if (m_Timer > NAME_HOLD)
        alpha = 1.0f - (m_Timer - NAME_HOLD) / NAME_FADE;

    float width = 420.0f;
    float height = width * (BANNER_H / BANNER_W);
    float x = (SCREEN_WIDTH - width) * 0.5f;
    float y = 40.0f;

    DrawBanner(x, y, width, height, XMFLOAT4(1.0f, 1.0f, 1.0f, alpha));
    Font::DrawCentered(GetStageData(stage).Name, SCREEN_WIDTH * 0.5f, y + 8.0f, 26.0f,
        XMFLOAT4(1.0f, 1.0f, 1.0f, alpha));
}
