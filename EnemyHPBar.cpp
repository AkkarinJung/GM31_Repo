#include "main.h"
#include "renderer.h"
#include "EnemyHPBar.h"
#include "manager.h"
#include "Camera.h"
#include "Enemy.h"
#include "Stats.h"

static const float BAR_WIDTH = 56.0f;   // screen pixels
static const float BAR_HEIGHT = 7.0f;
static const float BAR_BORDER = 1.0f;
static const float BAR_HEIGHT_OFFSET = 2.2f; // world units above the enemy's origin

void EnemyHPBar::Init()
{
    m_Layer = 4; // same UI layer as Score/HPBar/DamageNumber

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
}

void EnemyHPBar::Uninit()
{
    m_VertexBuffer->Release();
    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();
}

void EnemyHPBar::DrawQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color)
{
    MATERIAL material{};
    material.Diffuse = Color;
    material.TextureEnable = false;
    Renderer::SetMaterial(material);

    D3D11_MAPPED_SUBRESOURCE msr{};
    if (SUCCEEDED(Renderer::GetDeviceContext()->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
    {
        VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;

        vertex[0].Position = XMFLOAT3(X, Y, 0.0f);
        vertex[0].Normal = XMFLOAT3(0, 0, 0);
        vertex[0].Diffuse = XMFLOAT4(1, 1, 1, 1); // tinted through Material.Diffuse
        vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

        vertex[1].Position = XMFLOAT3(X + Width, Y, 0.0f);
        vertex[1].Normal = XMFLOAT3(0, 0, 0);
        vertex[1].Diffuse = XMFLOAT4(1, 1, 1, 1);
        vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

        vertex[2].Position = XMFLOAT3(X, Y + Height, 0.0f);
        vertex[2].Normal = XMFLOAT3(0, 0, 0);
        vertex[2].Diffuse = XMFLOAT4(1, 1, 1, 1);
        vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

        vertex[3].Position = XMFLOAT3(X + Width, Y + Height, 0.0f);
        vertex[3].Normal = XMFLOAT3(0, 0, 0);
        vertex[3].Diffuse = XMFLOAT4(1, 1, 1, 1);
        vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

        Renderer::GetDeviceContext()->Unmap(m_VertexBuffer, 0);
    }

    Renderer::GetDeviceContext()->Draw(4, 0);
}

void EnemyHPBar::Draw()
{
    // Nothing to point at while a menu has the game paused.
    if (Manager::IsPause())
        return;

    Camera* camera = Manager::GetGameObj<Camera>();
    if (camera == nullptr)
        return;

    auto enemies = Manager::GetGameObjs<Enemy>();
    if (enemies.size() == 0)
        return;

    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    Renderer::SetWorldViewProjection2D();
    Renderer::SetWorldMatrix(XMMatrixIdentity());

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    for (auto enemy : enemies)
    {
        Stats* stats = enemy->GetGameComponent<Stats>();
        if (stats == nullptr || stats->GetMaxHP() <= 0)
            continue;

        Vector3 position = enemy->GetPosition();
        position.y += BAR_HEIGHT_OFFSET;

        // world -> screen, the same projection DamageNumber uses
        XMVECTOR worldPos = XMVectorSet(position.x, position.y, position.z, 1.0f);
        XMVECTOR screenPos = XMVector3Project(worldPos,
            0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 1.0f,
            camera->GetProjectionMatrix(), camera->GetViewMatrix(), XMMatrixIdentity());

        XMFLOAT3 screen;
        XMStoreFloat3(&screen, screenPos);

        // Behind the camera: the projection mirrors those points back into
        // view, so they would draw as bars floating over nothing.
        if (screen.z < 0.0f || screen.z > 1.0f)
            continue;

        float ratio = (float)stats->GetHP() / (float)stats->GetMaxHP();
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;

        float x = screen.x - BAR_WIDTH * 0.5f;
        float y = screen.y - BAR_HEIGHT * 0.5f;

        // border, empty track, then the remaining HP on top
        DrawQuad(x - BAR_BORDER, y - BAR_BORDER,
            BAR_WIDTH + BAR_BORDER * 2.0f, BAR_HEIGHT + BAR_BORDER * 2.0f,
            XMFLOAT4(0.05f, 0.05f, 0.07f, 0.85f));

        DrawQuad(x, y, BAR_WIDTH, BAR_HEIGHT, XMFLOAT4(0.25f, 0.10f, 0.10f, 0.85f));

        if (ratio > 0.0f)
            DrawQuad(x, y, BAR_WIDTH * ratio, BAR_HEIGHT, XMFLOAT4(0.85f, 0.20f, 0.20f, 1.0f));
    }
}
