#include "main.h"
#include "renderer.h"
#include "DamageNumber.h"
#include "manager.h"
#include "Camera.h"

// How far apart the digits sit, against the size they are drawn at. The sheet
// leaves a margin round every glyph, so a full digitSize of pitch reads as a
// gap - under 1 closes it up without the art colliding.
#define DIGIT_SPACING (0.70f)

#define SPRITE_ROW (5)
#define SPRITE_COLUMNS (5)

void DamageNumber::Init(const Vector3& WorldPosition, int Value, bool ShowSign, const XMFLOAT4& Color)
{
    m_Layer = 4; // same UI layer as Score/HPBar - Manager::Draw only walks layers 0-4
    m_Value = Value;
    m_ShowSign = ShowSign;
    m_Color = Color;

    Camera* camera = Manager::GetGameObj<Camera>();
    if (camera == nullptr)
    {
        SetDestory();
        return;
    }

    XMVECTOR worldPos = XMVectorSet(WorldPosition.x, WorldPosition.y, WorldPosition.z, 1.0f);
    XMVECTOR screenPos = XMVector3Project(worldPos,
        0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, 0.0f, 1.0f,
        camera->GetProjectionMatrix(), camera->GetViewMatrix(), XMMatrixIdentity());

    XMFLOAT3 screen;
    XMStoreFloat3(&screen, screenPos);

    // Behind the camera: the projection mirrors those points back into view,
    // so a kill just off screen drew its number over unrelated geometry.
    // EnemyHPBar guards this; this did not.
    if (screen.z < 0.0f || screen.z > 1.0f)
    {
        SetDestory();
        return;
    }

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
    LoadFromWICFile(L"asset\\texture\\number_gold.png", WIC_FLAGS_NONE, &metadata, image);
    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &m_Texture);
    assert(m_Texture);
}

void DamageNumber::Uninit()
{
    // Init can bail out before creating anything (no camera, or the world
    // position is behind it), and the object is destroyed that same frame -
    // so none of these are guaranteed to exist.
    if (m_VertexBuffer != nullptr) m_VertexBuffer->Release();
    if (m_VertexLayout != nullptr) m_VertexLayout->Release();
    if (m_VertexShader != nullptr) m_VertexShader->Release();
    if (m_PixelShader != nullptr) m_PixelShader->Release();
    if (m_Texture != nullptr) m_Texture->Release();
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
    const float digitAdvance = digitSize * DIGIT_SPACING;

    // The quad used to run from the anchor downwards, so the number hung below
    // the point it was given. Sit it above instead - the anchor is the bottom
    // of the number now, which is where a popup over a head wants to be.
    const float digitTop = -digitSize;

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

    // The last digit still takes its full width; only the pitch between them
    // shrinks. Getting this wrong puts the number off centre as it grows.
    float digitsWidth = digitAdvance * (digitCount - 1) + digitSize;
    float totalWidth = digitsWidth + (drawSign ? signWidth + signGap : 0.0f);
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
        float x = digitStartX + i * digitAdvance;

        D3D11_MAPPED_SUBRESOURCE msr{};
        if (SUCCEEDED(Renderer::GetDeviceContext()->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
        {
            VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;

            vertex[0].Position = XMFLOAT3(x, digitTop, 0.0f);
            vertex[0].Normal = XMFLOAT3(0, 0, 0);
            vertex[0].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[0].TexCoord = XMFLOAT2(u, v);

            vertex[1].Position = XMFLOAT3(x + digitSize, digitTop, 0.0f);
            vertex[1].Normal = XMFLOAT3(0, 0, 0);
            vertex[1].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[1].TexCoord = XMFLOAT2(u + w, v);

            vertex[2].Position = XMFLOAT3(x, digitTop + digitSize, 0.0f);
            vertex[2].Normal = XMFLOAT3(0, 0, 0);
            vertex[2].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[2].TexCoord = XMFLOAT2(u, v + h);

            vertex[3].Position = XMFLOAT3(x + digitSize, digitTop + digitSize, 0.0f);
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
        float midY = digitTop + digitSize * 0.5f; // follows the digits up

        DrawFlatQuad(startX, midY - barThickness * 0.5f, signWidth, barThickness); // horizontal bar (both + and -)

        if (!negative)
        {
            DrawFlatQuad(startX + signWidth * 0.5f - barThickness * 0.5f,
                midY - signWidth * 0.5f, barThickness, signWidth); // vertical bar completes the +
        }
    }
}