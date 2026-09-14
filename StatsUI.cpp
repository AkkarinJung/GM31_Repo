#include <stdio.h>

#include "main.h"
#include "renderer.h"
#include "StatsUI.h"
#include "ControlsUI.h"
#include "manager.h"
#include "input.h"
#include "Font.h"
#include "Player.h"
#include "Stats.h"
#include "Weapon.h"
#include "Game.h"
#include "RoguelikeSystem.h"

// Prompts come off the same 544x384 sheet ControlsUI uses: 16x16 tiles, so a
// prompt is a column, a row, and how many tiles wide it is.
static const float INPUT_SHEET_WIDTH = 544.0f;
static const float INPUT_SHEET_HEIGHT = 384.0f;
static const float INPUT_TILE = 16.0f;

// The sheet is laid out as a QWERTY keyboard, so I sits in the top letter row.
static const int KEY_I_COL = 24;
static const int KEY_I_ROW = 2;

// Panel layout, in screen pixels.
static const float PANEL_WIDTH = 520.0f;
static const float ROW_HEIGHT = 38.0f;
static const float LABEL_X = 40.0f;   // from the panel's left edge
static const float VALUE_X = 330.0f;  // where the numbers line up
static const float BAR_WIDTH = 150.0f;
static const float BAR_HEIGHT = 12.0f;

// The banner on the card sheet, the same header ControlsUI borrows.
static const float BANNER_X = 64.0f, BANNER_Y = 827.0f, BANNER_W = 800.0f, BANNER_H = 77.0f;
static const float CARD_SHEET_WIDTH = 1536.0f;
static const float CARD_SHEET_HEIGHT = 1024.0f;

static const XMFLOAT4 COLOUR_WHITE = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
static const XMFLOAT4 COLOUR_LABEL = XMFLOAT4(0.72f, 0.76f, 0.85f, 1.0f);
static const XMFLOAT4 COLOUR_VALUE = XMFLOAT4(1.0f, 0.96f, 0.80f, 1.0f);
static const XMFLOAT4 COLOUR_HP = XMFLOAT4(0.85f, 0.25f, 0.30f, 1.0f);
static const XMFLOAT4 COLOUR_MP = XMFLOAT4(0.30f, 0.55f, 0.90f, 1.0f);
static const XMFLOAT4 COLOUR_REWARD = XMFLOAT4(0.60f, 0.90f, 0.65f, 1.0f);


void StatsUI::Init()
{
    m_Layer = 4; // same UI layer as Score / HPBar / ControlsUI

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

    TexMetadata inputMetadata;
    ScratchImage inputImage;
    LoadFromWICFile(L"asset\\texture\\Input\\tilemap_packed.png", WIC_FLAGS_NONE,
        &inputMetadata, inputImage);
    CreateShaderResourceView(Renderer::GetDevice(), inputImage.GetImages(),
        inputImage.GetImageCount(), inputMetadata, &m_InputTexture);
    assert(m_InputTexture);

    TexMetadata panelMetadata;
    ScratchImage panelImage;
    LoadFromWICFile(L"asset\\texture\\Simple_card_Design.png", WIC_FLAGS_NONE,
        &panelMetadata, panelImage);
    CreateShaderResourceView(Renderer::GetDevice(), panelImage.GetImages(),
        panelImage.GetImageCount(), panelMetadata, &m_PanelTexture);
    assert(m_PanelTexture);
}

void StatsUI::Uninit()
{
    m_VertexBuffer->Release();
    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();
    m_InputTexture->Release();
    m_PanelTexture->Release();
}

void StatsUI::Update()
{
    if (Input::GetKeyTrigger('I'))
    {
        m_Open = !m_Open;

        // Both panels sit in the middle of the screen, so only one at a time.
        if (m_Open)
        {
            ControlsUI* controls = Manager::GetGameObj<ControlsUI>();
            if (controls)
                controls->SetOpen(false);
        }
    }

    GameObject::Update();
}

void StatsUI::BindPipeline()
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

void StatsUI::DrawFlatQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color)
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

void StatsUI::DrawSprite(ID3D11ShaderResourceView* Texture, float SheetWidth, float SheetHeight,
    float SourceX, float SourceY, float SourceWidth, float SourceHeight,
    float X, float Y, float Width, float Height)
{
    BindPipeline();

    MATERIAL material{};
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.TextureEnable = true;
    Renderer::SetMaterial(material);

    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &Texture);

    // pixels in the sheet -> 0-1 texture coordinates
    float u = SourceX / SheetWidth;
    float v = SourceY / SheetHeight;
    float uWidth = SourceWidth / SheetWidth;
    float vHeight = SourceHeight / SheetHeight;

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

float StatsUI::DrawStatRow(float PanelX, float Y, const char* Label, const char* Value)
{
    Font::Draw(Label, PanelX + LABEL_X, Y, 22.0f, COLOUR_LABEL);
    Font::Draw(Value, PanelX + VALUE_X, Y, 22.0f, COLOUR_VALUE);

    return Y + ROW_HEIGHT;
}

float StatsUI::DrawBarRow(float PanelX, float Y, const char* Label, const char* Value,
    float Fraction, const XMFLOAT4& Color)
{
    Font::Draw(Label, PanelX + LABEL_X, Y, 22.0f, COLOUR_LABEL);
    Font::Draw(Value, PanelX + VALUE_X, Y, 22.0f, COLOUR_VALUE);

    if (Fraction < 0.0f) Fraction = 0.0f;
    if (Fraction > 1.0f) Fraction = 1.0f;

    // Bar under the text, so a glance reads the ratio without the numbers.
    float barX = PanelX + LABEL_X;
    float barY = Y + 26.0f;

    DrawFlatQuad(barX, barY, BAR_WIDTH, BAR_HEIGHT, XMFLOAT4(0.18f, 0.18f, 0.22f, 1.0f));
    DrawFlatQuad(barX, barY, BAR_WIDTH * Fraction, BAR_HEIGHT, Color);

    return Y + ROW_HEIGHT + 14.0f;
}

void StatsUI::Draw()
{
    // While the reward cards are up the game is paused and they own the
    // screen - stay out of the way, the same as ControlsUI does.
    if (Manager::IsPause())
        return;

    if (!m_Open)
    {
        // closed - just the hint, so the panel is discoverable. Sits above
        // the CONTROLS hint in the same corner.
        float hintX = 24.0f;
        float hintY = SCREEN_HEIGHT - 84.0f;

        DrawSprite(m_InputTexture, INPUT_SHEET_WIDTH, INPUT_SHEET_HEIGHT,
            KEY_I_COL * INPUT_TILE, KEY_I_ROW * INPUT_TILE, INPUT_TILE, INPUT_TILE,
            hintX, hintY, 28.0f, 28.0f);

        Font::Draw("STATS", hintX + 66.0f, hintY + 4.0f, 20.0f,
            XMFLOAT4(1.0f, 1.0f, 1.0f, 0.75f));
        return;
    }

    Player* player = Manager::GetGameObj<Player>();
    Stats* stats = player ? player->GetGameComponent<Stats>() : nullptr;
    Weapon* weapon = player ? player->GetWeapon() : nullptr;

    const std::vector<RoguelikeReward>& taken = RoguelikeSystem::GetTaken();
    int rewardCount = (int)taken.size();

    // Seven stat rows, two of which carry a bar, then the rewards.
    float panelHeight = 150.0f + 7.0f * ROW_HEIGHT + 2.0f * 14.0f
        + (rewardCount > 0 ? (34.0f + rewardCount * 26.0f) : 0.0f);

    float panelX = (SCREEN_WIDTH - PANEL_WIDTH) * 0.5f;
    float panelY = (SCREEN_HEIGHT - panelHeight) * 0.5f;

    DrawFlatQuad(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT,
        XMFLOAT4(0.0f, 0.0f, 0.0f, 0.55f));

    DrawFlatQuad(panelX, panelY, PANEL_WIDTH, panelHeight,
        XMFLOAT4(0.10f, 0.10f, 0.14f, 0.95f));

    // header banner, the same one the controls panel uses so both match.
    float bannerWidth = PANEL_WIDTH - 60.0f;
    float bannerHeight = bannerWidth * (BANNER_H / BANNER_W);

    DrawSprite(m_PanelTexture, CARD_SHEET_WIDTH, CARD_SHEET_HEIGHT,
        BANNER_X, BANNER_Y, BANNER_W, BANNER_H,
        panelX + 30.0f, panelY - bannerHeight * 0.5f, bannerWidth, bannerHeight);

    Font::DrawCentered("STATS", panelX + PANEL_WIDTH * 0.5f, panelY - 12.0f, 24.0f, COLOUR_WHITE);

    char value[64];
    float rowY = panelY + 60.0f;

    snprintf(value, sizeof(value), "%d", Game::GetStageIndex() + 1);
    rowY = DrawStatRow(panelX, rowY, "STAGE", value);

    if (stats)
    {
        snprintf(value, sizeof(value), "%d / %d", stats->GetHP(), stats->GetMaxHP());
        rowY = DrawBarRow(panelX, rowY, "HP", value,
            stats->GetMaxHP() > 0 ? (float)stats->GetHP() / stats->GetMaxHP() : 0.0f,
            COLOUR_HP);

        snprintf(value, sizeof(value), "%d / %d", stats->GetMP(), stats->GetMaxMP());
        rowY = DrawBarRow(panelX, rowY, "MP", value,
            stats->GetMaxMP() > 0 ? (float)stats->GetMP() / stats->GetMaxMP() : 0.0f,
            COLOUR_MP);

        snprintf(value, sizeof(value), "%d", stats->GetAttack());
        rowY = DrawStatRow(panelX, rowY, "ATTACK", value);

        snprintf(value, sizeof(value), "%d", stats->GetDefense());
        rowY = DrawStatRow(panelX, rowY, "DEFENSE", value);

        snprintf(value, sizeof(value), "%d%%", (int)(stats->GetCriticalChance() * 100.0f + 0.5f));
        rowY = DrawStatRow(panelX, rowY, "CRITICAL", value);
    }
    else
    {
        rowY = DrawStatRow(panelX, rowY, "HP", "-");
    }

    if (weapon)
    {
        snprintf(value, sizeof(value), "%.0f", weapon->GetDamage() * weapon->GetDamageMultiplier());
        rowY = DrawStatRow(panelX, rowY, "WEAPON DAMAGE", value);

        snprintf(value, sizeof(value), "%.1f", weapon->GetRange());
        rowY = DrawStatRow(panelX, rowY, "WEAPON RANGE", value);
    }

    if (rewardCount > 0)
    {
        rowY += 8.0f;
        Font::Draw("RUN REWARDS", panelX + LABEL_X, rowY, 20.0f, COLOUR_WHITE);
        rowY += 26.0f;

        for (int i = 0; i < rewardCount; i++)
        {
            Font::Draw(taken[i].Name, panelX + LABEL_X + 14.0f, rowY, 19.0f, COLOUR_REWARD);
            rowY += 26.0f;
        }
    }

    DrawSprite(m_InputTexture, INPUT_SHEET_WIDTH, INPUT_SHEET_HEIGHT,
        KEY_I_COL * INPUT_TILE, KEY_I_ROW * INPUT_TILE, INPUT_TILE, INPUT_TILE,
        panelX + 40.0f, panelY + panelHeight - 42.0f, 26.0f, 26.0f);

    Font::Draw("CLOSE", panelX + 100.0f, panelY + panelHeight - 38.0f, 20.0f,
        XMFLOAT4(0.65f, 0.68f, 0.75f, 1.0f));
}
