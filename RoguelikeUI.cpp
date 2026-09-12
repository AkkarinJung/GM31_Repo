#include "main.h"
#include "renderer.h"
#include "RoguelikeUI.h"
#include "RoguelikeSystem.h"

#define SPRITE_ROW (5)
#define SPRITE_COLUMNS (5)

// Card layout, in screen pixels.
static const float CARD_WIDTH = 260.0f;
static const float CARD_HEIGHT = 320.0f;
static const float CARD_GAP = 30.0f;
static const float CARD_TOP = 200.0f;

void RoguelikeUI::Init()
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

    TexMetadata metadata;
    ScratchImage image;
    LoadFromWICFile(L"asset\\texture\\number.png", WIC_FLAGS_NONE, &metadata, image);
    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &m_Texture);
    assert(m_Texture);
}

void RoguelikeUI::Uninit()
{
    m_VertexBuffer->Release();
    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();
    m_Texture->Release();
}

void RoguelikeUI::SetSystem(RoguelikeSystem* System)
{
    m_System = System;

    // No font in the project, so the readable version of the cards goes to
    // the debug output - handy while tuning the reward pool.
    if (m_System == nullptr)
        return;

    char buffer[256];
    OutputDebugStringA("=== Roguelike: choose a reward ===\n");

    const std::vector<RoguelikeReward>& choices = m_System->GetChoices();
    for (int i = 0; i < (int)choices.size(); i++)
    {
        sprintf_s(buffer, "[%d] %s : %s\n", i + 1,
            choices[i].Category == RewardCategory::Common ? "Common" : "Weapon",
            choices[i].Name);
        OutputDebugStringA(buffer);
    }
}

void RoguelikeUI::DrawQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color)
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

void RoguelikeUI::DrawNumber(int Value, float CenterX, float Y, float DigitSize, const XMFLOAT4& Color)
{
    MATERIAL material{};
    material.Diffuse = Color;
    material.TextureEnable = true;
    Renderer::SetMaterial(material);

    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);

    const float w = 1.0f / (float)SPRITE_COLUMNS;
    const float h = 1.0f / (float)SPRITE_ROW;

    // up to 3 digits, least-significant first
    int number = Value < 0 ? -Value : Value;
    int digits[3];
    int digitCount = 0;
    do
    {
        digits[digitCount++] = number % 10;
        number /= 10;
    } while (number > 0 && digitCount < 3);

    float startX = CenterX - (DigitSize * digitCount) * 0.5f;

    for (int i = 0; i < digitCount; i++)
    {
        int digit = digits[digitCount - 1 - i]; // most-significant first
        float u = (digit % SPRITE_COLUMNS) * w;
        float v = (digit / SPRITE_COLUMNS) * h;
        float x = startX + i * DigitSize;

        D3D11_MAPPED_SUBRESOURCE msr{};
        if (SUCCEEDED(Renderer::GetDeviceContext()->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
        {
            VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;

            vertex[0].Position = XMFLOAT3(x, Y, 0.0f);
            vertex[0].Normal = XMFLOAT3(0, 0, 0);
            vertex[0].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[0].TexCoord = XMFLOAT2(u, v);

            vertex[1].Position = XMFLOAT3(x + DigitSize, Y, 0.0f);
            vertex[1].Normal = XMFLOAT3(0, 0, 0);
            vertex[1].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[1].TexCoord = XMFLOAT2(u + w, v);

            vertex[2].Position = XMFLOAT3(x, Y + DigitSize, 0.0f);
            vertex[2].Normal = XMFLOAT3(0, 0, 0);
            vertex[2].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[2].TexCoord = XMFLOAT2(u, v + h);

            vertex[3].Position = XMFLOAT3(x + DigitSize, Y + DigitSize, 0.0f);
            vertex[3].Normal = XMFLOAT3(0, 0, 0);
            vertex[3].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[3].TexCoord = XMFLOAT2(u + w, v + h);

            Renderer::GetDeviceContext()->Unmap(m_VertexBuffer, 0);
        }

        Renderer::GetDeviceContext()->Draw(4, 0);
    }
}

void RoguelikeUI::GetCardRect(int Index, int Count, float& X, float& Y, float& Width, float& Height) const
{
    float totalWidth = Count * CARD_WIDTH + (Count - 1) * CARD_GAP;
    float startX = (SCREEN_WIDTH - totalWidth) * 0.5f;

    X = startX + Index * (CARD_WIDTH + CARD_GAP);
    Y = CARD_TOP;
    Width = CARD_WIDTH;
    Height = CARD_HEIGHT;
}

int RoguelikeUI::GetCardIndexAt(float X, float Y) const
{
    if (m_System == nullptr)
        return -1;

    int count = (int)m_System->GetChoices().size();

    for (int i = 0; i < count; i++)
    {
        float cardX, cardY, cardWidth, cardHeight;
        GetCardRect(i, count, cardX, cardY, cardWidth, cardHeight);

        if (X >= cardX && X <= cardX + cardWidth &&
            Y >= cardY && Y <= cardY + cardHeight)
            return i;
    }

    return -1;
}

void RoguelikeUI::Draw()
{
    if (m_System == nullptr || !m_System->IsSelecting())
        return;

    const std::vector<RoguelikeReward>& choices = m_System->GetChoices();
    int count = (int)choices.size();
    if (count <= 0)
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

    // dim the map behind the cards
    DrawQuad(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.6f));

    float totalWidth = count * CARD_WIDTH + (count - 1) * CARD_GAP;
    float startX = (SCREEN_WIDTH - totalWidth) * 0.5f;

    for (int i = 0; i < count; i++)
    {
        const RoguelikeReward& reward = choices[i];

        bool common = reward.Category == RewardCategory::Common;
        XMFLOAT4 categoryColor = common
            ? XMFLOAT4(0.25f, 0.55f, 0.95f, 1.0f)  // Common - blue
            : XMFLOAT4(0.90f, 0.35f, 0.25f, 1.0f); // Weapon - orange/red

        float x, y, width, height;
        GetCardRect(i, count, x, y, width, height);

        bool hovered = (i == m_HoveredIndex);

        // hovered card gets a colored outline and a lighter body, so the
        // cursor makes it obvious what a click would take
        if (hovered)
            DrawQuad(x - 6.0f, y - 6.0f, width + 12.0f, height + 12.0f, categoryColor);

        XMFLOAT4 bodyColor = hovered
            ? XMFLOAT4(0.18f, 0.18f, 0.24f, 1.0f)
            : XMFLOAT4(0.10f, 0.10f, 0.14f, 0.95f);

        DrawQuad(x, y, width, height, bodyColor);                              // card body
        DrawQuad(x + 6.0f, y + 6.0f, width - 12.0f, 60.0f, categoryColor);     // category header

        // the amount: a percent reward shows its percentage (0.15f -> 15)
        int amount = reward.Percent ? (int)(reward.Value * 100.0f + 0.5f) : (int)reward.Value;
        DrawNumber(amount, x + width * 0.5f, y + 130.0f, 60.0f, XMFLOAT4(1, 1, 1, 1));

        // no '%' glyph in the spritesheet - a bar under the number marks a
        // percentage reward, nothing under it means a flat amount
        if (reward.Percent)
            DrawQuad(x + width * 0.5f - 40.0f, y + 200.0f, 80.0f, 6.0f, categoryColor);

        // card number - matches the list printed to the debug output
        DrawNumber(i + 1, x + width * 0.5f, y + height - 80.0f, 40.0f, categoryColor);
    }
}