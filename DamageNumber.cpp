#include "main.h"
#include "renderer.h"
#include "DamageNumber.h"
#include "manager.h"
#include "Camera.h"

#define SPRITE_ROW (5)
#define SPRITE_COLUMNS (5)

void DamageNumber::Init(const Vector3& WorldPosition, int Value, bool ShowSign, const XMFLOAT4& Color)
{
    m_Layer = 4; // same UI layer as Score/HPBar/MPBar - Manager::Draw only walks layers 0-4
    m_Value = Value;
    m_ShowSign = ShowSign;
    m_Color = Color;

    Camera* camera = Manager::GetGameObj<Camera>();

    XMVECTOR worldPos = XMVectorSet(WorldPosition.x, WorldPosition.y, WorldPosition.z, 1.0f);
    XMVECTOR screenPos = XMVector3Project(worldPos,
        0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 1.0f,
        camera->GetProjectionMatrix(), camera->GetViewMatrix(), XMMatrixIdentity());

    XMFLOAT3 screen;
    XMStoreFloat3(&screen, screenPos);
    m_Position = { screen.x, screen.y, 0.0f };

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
    LoadFromWICFile(L"asset\\texture\\number.png", WIC_FLAGS_NONE, &metadata, image);
    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &m_Texture);
    assert(m_Texture);
}

void DamageNumber::Uninit()
{
    m_VertexBuffer->Release();
    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();
    m_Texture->Release();
}

void DamageNumber::Update()
{
    const float dt = 1.0f / 60.0f;

    m_Timer += dt;
    m_Position.y -= m_RiseSpeed * dt; // screen-space Y increases downward, so this drifts up

    if (m_Timer >= m_Lifetime)
        SetDestory();
}

void DamageNumber::DrawFlatQuad(float X, float Y, float Width, float Height)
{
    D3D11_MAPPED_SUBRESOURCE msr{};
    if (SUCCEEDED(Renderer::GetDeviceContext()->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
    {
        VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;

        vertex[0].Position = XMFLOAT3(X, Y, 0.0f);
        vertex[0].Normal = XMFLOAT3(0, 0, 0);
        vertex[0].Diffuse = XMFLOAT4(1, 1, 1, 1); // tinted via Material.Diffuse in the vertex shader
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

void DamageNumber::Draw()
{
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    Renderer::SetWorldViewProjection2D();
    Renderer::SetWorldMatrix(XMMatrixTranslation(m_Position.x, m_Position.y, 0.0f));

    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);

    const float w = 1.0f / (float)SPRITE_COLUMNS;
    const float h = 1.0f / (float)SPRITE_ROW;
    const float digitSize = 32.0f;// <--- SIZE

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    bool negative = m_Value < 0;
    bool drawSign = negative || m_ShowSign;

    // extract up to 3 significant decimal digits, least-significant first
    int number = negative ? -m_Value : m_Value;
    int digits[3];
    int digitCount = 0;
    do
    {
        digits[digitCount++] = number % 10;
        number /= 10;
    } while (number > 0 && digitCount < 3);

    const float signWidth = digitSize * 0.6f;
    const float signGap = digitSize * 0.15f;
    float totalWidth = digitSize * digitCount + (drawSign ? signWidth + signGap : 0.0f);
    float startX = -totalWidth * 0.5f;
    float digitStartX = startX + (drawSign ? signWidth + signGap : 0.0f);

    MATERIAL material{};
    material.Diffuse = m_Color;
    material.TextureEnable = true;
    Renderer::SetMaterial(material);

    for (int i = 0; i < digitCount; i++)
    {
        int digit = digits[digitCount - 1 - i]; // most-significant first
        float u = (digit % SPRITE_COLUMNS) * w;
        float v = (digit / SPRITE_COLUMNS) * h;
        float x = digitStartX + i * digitSize;

        D3D11_MAPPED_SUBRESOURCE msr{};
        if (SUCCEEDED(Renderer::GetDeviceContext()->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
        {
            VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;

            vertex[0].Position = XMFLOAT3(x, 0.0f, 0.0f);
            vertex[0].Normal = XMFLOAT3(0, 0, 0);
            vertex[0].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[0].TexCoord = XMFLOAT2(u, v);

            vertex[1].Position = XMFLOAT3(x + digitSize, 0.0f, 0.0f);
            vertex[1].Normal = XMFLOAT3(0, 0, 0);
            vertex[1].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[1].TexCoord = XMFLOAT2(u + w, v);

            vertex[2].Position = XMFLOAT3(x, digitSize, 0.0f);
            vertex[2].Normal = XMFLOAT3(0, 0, 0);
            vertex[2].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[2].TexCoord = XMFLOAT2(u, v + h);

            vertex[3].Position = XMFLOAT3(x + digitSize, digitSize, 0.0f);
            vertex[3].Normal = XMFLOAT3(0, 0, 0);
            vertex[3].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[3].TexCoord = XMFLOAT2(u + w, v + h);

            Renderer::GetDeviceContext()->Unmap(m_VertexBuffer, 0);
        }

        Renderer::GetDeviceContext()->Draw(4, 0);
    }

    if (drawSign)
    {
        // number.png has no +/- glyph - draw the sign as flat colored bars instead
        material.TextureEnable = false;
        Renderer::SetMaterial(material);

        float barThickness = digitSize * 0.15f;
        float midY = digitSize * 0.5f;

        DrawFlatQuad(startX, midY - barThickness * 0.5f, signWidth, barThickness); // horizontal bar (both + and -)

        if (!negative)
        {
            DrawFlatQuad(startX + signWidth * 0.5f - barThickness * 0.5f,
                midY - signWidth * 0.5f, barThickness, signWidth); // vertical bar completes the +
        }
    }
}