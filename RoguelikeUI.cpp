#include "main.h"
#include "renderer.h"
#include "RoguelikeUI.h"
#include "RoguelikeSystem.h"
#include "Font.h"

// A piece of asset\texture\Simple_card_Design.png, in pixels of that sheet.
struct SpriteRect
{
    float X, Y, Width, Height;
};

static const float SHEET_WIDTH = 1536.0f;
static const float SHEET_HEIGHT = 1024.0f;

// Frame colour is RARITY, not category. The sheet carries four frames and
// four matching diamonds - grey, blue, purple, gold - which is exactly the
// four tiers, so nothing had to be drawn for this.
//
// The frames used to mean category instead (blue = player, gold = weapon),
// which wasted half the art and collided with itself the moment rarity
// existed: "Common" was both a category and a tier. Category is the word
// printed on the card now, and it reads PLAYER or WEAPON.
//
// Rects measured off the sheet itself rather than eyeballed.
static const SpriteRect CARD_BY_RARITY[(int)RewardRarity::Count] =
{
    {  734.0f, 110.0f, 243.0f, 285.0f }, // Common    - grey
    { 1010.0f, 110.0f, 237.0f, 287.0f }, // Rare      - blue
    {  734.0f, 428.0f, 243.0f, 285.0f }, // Epic      - purple
    { 1010.0f, 428.0f, 237.0f, 285.0f }, // Legendary - gold
};

static const SpriteRect ICON_BY_RARITY[(int)RewardRarity::Count] =
{
    { 1358.0f, 180.0f,  97.0f,  99.0f },
    { 1358.0f, 304.0f,  97.0f,  99.0f },
    { 1356.0f, 428.0f, 101.0f, 101.0f },
    { 1354.0f, 552.0f, 103.0f, 103.0f },
};

// The category line, tinted to match its frame so the card reads as one
// piece rather than as a grey label stuck on gold.
static const XMFLOAT4 TINT_BY_RARITY[(int)RewardRarity::Count] =
{
    { 0.78f, 0.76f, 0.74f, 1.0f }, // Common
    { 0.45f, 0.72f, 1.00f, 1.0f }, // Rare
    { 0.72f, 0.50f, 1.00f, 1.0f }, // Epic
    { 1.00f, 0.80f, 0.30f, 1.0f }, // Legendary
};

static const SpriteRect BANNER      = {   64.0f, 827.0f, 800.0f,  77.0f };

// Card layout, in screen pixels. The art is 240x289, so the card keeps that
// shape - stretching it would soften the border.
static const float CARD_WIDTH = 260.0f;
static const float CARD_HEIGHT = 313.0f;
static const float CARD_GAP = 36.0f;
static const float CARD_TOP = 210.0f;

// The frame art has a darker panel across its bottom third - the reward
// name goes in there, everything else above it.
static const float FOOTER_START = 0.70f;

// How far the map behind the cards is dimmed. Named only so it is tunable -
// 0.65 is the value this screen has always used.
static const float DIM_STRENGTH = 0.65f;

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
    LoadFromWICFile(L"asset\\texture\\Simple_card_Design.png", WIC_FLAGS_NONE, &metadata, image);
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

    if (m_System == nullptr)
        return;

    char buffer[256];
    OutputDebugStringA("=== Roguelike: click a reward ===\n");

    const std::vector<RoguelikeReward>& choices = m_System->GetChoices();
    for (int i = 0; i < (int)choices.size(); i++)
    {
        sprintf_s(buffer, "[%d] %-9s %-6s : %s\n", i + 1,
            RarityName(choices[i].Rarity),
            choices[i].Category == RewardCategory::Common ? "PLAYER" : "WEAPON",
            choices[i].Name);
        OutputDebugStringA(buffer);
    }
}

void RoguelikeUI::BindPipeline()
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
}

void RoguelikeUI::DrawFlatQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color)
{
    BindPipeline();

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

void RoguelikeUI::DrawSprite(const SpriteRect& Source, float X, float Y, float Width, float Height,
    const XMFLOAT4& Color)
{
    BindPipeline();

    MATERIAL material{};
    material.Diffuse = Color;
    material.TextureEnable = true;
    Renderer::SetMaterial(material);

    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);

    // pixels in the sheet -> 0-1 texture coordinates
    float u = Source.X / SHEET_WIDTH;
    float v = Source.Y / SHEET_HEIGHT;
    float uWidth = Source.Width / SHEET_WIDTH;
    float vHeight = Source.Height / SHEET_HEIGHT;

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

void RoguelikeUI::DrawWrapped(const char* Text, float CenterX, float Y, float MaxWidth, float Size,
    const XMFLOAT4& Color)
{
    // Greedy wrap: keep adding words while the line still fits, then start
    // a new one. Reward names are short, so nothing fancier is needed.
    char line[128] = "";
    char candidate[128];
    char word[64];

    int lineLength = 0;
    float lineY = Y;

    const char* p = Text;

    while (*p != '\0')
    {
        while (*p == ' ')
            p++;

        int wordLength = 0;
        while (*p != '\0' && *p != ' ' && wordLength < 63)
            word[wordLength++] = *p++;
        word[wordLength] = '\0';

        if (wordLength == 0)
            break;

        if (lineLength > 0)
            sprintf_s(candidate, "%s %s", line, word);
        else
            sprintf_s(candidate, "%s", word);

        if (lineLength == 0 || Font::Measure(candidate, Size) <= MaxWidth)
        {
            lineLength = sprintf_s(line, "%s", candidate);
        }
        else
        {
            Font::DrawCentered(line, CenterX, lineY, Size, Color);
            lineY += Size * 1.15f;
            lineLength = sprintf_s(line, "%s", word);
        }
    }

    if (lineLength > 0)
        Font::DrawCentered(line, CenterX, lineY, Size, Color);
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

    const XMFLOAT4 white = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    // dim the map behind the cards
    DrawFlatQuad(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT,
        XMFLOAT4(0.0f, 0.0f, 0.0f, DIM_STRENGTH));

    // title banner
    DrawSprite(BANNER, SCREEN_WIDTH * 0.5f - 300.0f, 110.0f, 600.0f, 58.0f, white);
    Font::DrawCentered("CHOOSE A REWARD", SCREEN_WIDTH * 0.5f, 124.0f, 30.0f, white);

    for (int i = 0; i < count; i++)
    {
        const RoguelikeReward& reward = choices[i];

        int rarity = (int)reward.Rarity;
        if (rarity < 0 || rarity >= (int)RewardRarity::Count)
            rarity = 0;

        const SpriteRect& frame = CARD_BY_RARITY[rarity];
        const SpriteRect& icon = ICON_BY_RARITY[rarity];

        float x, y, width, height;
        GetCardRect(i, count, x, y, width, height);

        // The hovered card grows a little around its centre - the frame art
        // already glows, so a lift is enough to read as "this one".
        if (i == m_HoveredIndex)
        {
            float grow = 12.0f;
            x -= grow * 0.5f;
            y -= grow * 0.5f;
            width += grow;
            height += grow;
        }

        DrawSprite(frame, x, y, width, height, white);

        float centerX = x + width * 0.5f;
        float footerY = y + height * FOOTER_START;

        // category icon, sitting in the open upper part of the frame
        float iconSize = width * 0.34f;
        DrawSprite(icon, centerX - iconSize * 0.5f, y + height * 0.16f, iconSize, iconSize, white);

        // Category, in the frame's own colour. The frame says how rare the
        // card is; this says what it touches.
        Font::DrawCentered(
            reward.Category == RewardCategory::Common ? "PLAYER" : "WEAPON",
            centerX, y + height * 0.52f, 20.0f, TINT_BY_RARITY[rarity]);

        // the reward name, wrapped inside the darker footer panel
        DrawWrapped(reward.Name, centerX, footerY + 18.0f, width - 34.0f, 21.0f, white);
    }
}
