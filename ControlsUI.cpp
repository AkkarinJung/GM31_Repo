#include "main.h"
#include "renderer.h"
#include "ControlsUI.h"
#include "manager.h"
#include "input.h"
#include "Font.h"

// One prompt on asset\texture\Input\tilemap_packed.png: a 544x384 sheet of
// 16x16 tiles, so a prompt is a column/row and how many tiles wide it is.
struct InputSprite
{
    int Col, Row, Tiles;
};

static const float INPUT_SHEET_WIDTH = 544.0f;
static const float INPUT_SHEET_HEIGHT = 384.0f;
static const float INPUT_TILE = 16.0f;

static const InputSprite KEY_NONE  = { -1, 0, 0 };
static const InputSprite KEY_A     = { 18, 3, 1 };
static const InputSprite KEY_D     = { 20, 3, 1 };
static const InputSprite KEY_SPACE = { 31, 6, 3 };
static const InputSprite KEY_ESC   = { 17, 0, 1 };
static const InputSprite KEY_TAB   = { 19, 5, 2 }; // 2 tiles wide
static const InputSprite KEY_F1    = { 18, 0, 1 };
static const InputSprite MOUSE_LEFT  = {  9, 2, 1 };
static const InputSprite MOUSE_RIGHT = { 10, 2, 1 };

// The binding list. This table is the whole panel - add a line here and the
// row appears, no layout code to touch.
struct ControlEntry
{
    InputSprite First;
    InputSprite Second;
    const char* Label;
};

static const ControlEntry s_Controls[] =
{
    { KEY_A,       KEY_D,     "Move left / right" },
    { KEY_SPACE,   KEY_NONE,  "Jump" },
    { MOUSE_LEFT,  KEY_NONE,  "Attack" },
    { MOUSE_RIGHT, KEY_NONE,  "Special attack" },
    { KEY_F1,      KEY_NONE,  "Debug camera" },
    { KEY_ESC,     KEY_NONE,  "Quit" },
};

static const int s_ControlCount = (int)(sizeof(s_Controls) / sizeof(s_Controls[0]));

// Panel layout, in screen pixels.
static const float PROMPT_SIZE = 32.0f;  // 16px tiles drawn at 2x - an integer
                                         // scale, which keeps the pixel art from
                                         // smearing under the anisotropic sampler
static const float ROW_HEIGHT = 46.0f;
static const float PANEL_WIDTH = 500.0f;
static const float LABEL_X = 150.0f;     // from the panel's left edge, so every
                                         // label lines up whatever prompt it follows

// The banner on the card sheet, reused as this panel's header.
static const float BANNER_X = 64.0f, BANNER_Y = 827.0f, BANNER_W = 800.0f, BANNER_H = 77.0f;
static const float CARD_SHEET_WIDTH = 1536.0f;
static const float CARD_SHEET_HEIGHT = 1024.0f;

void ControlsUI::Init()
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

void ControlsUI::Uninit()
{
    m_VertexBuffer->Release();
    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();
    m_InputTexture->Release();
    m_PanelTexture->Release();
}

void ControlsUI::Update()
{
    // Not F1: Camera::Update uses that for its debug free-fly camera, so
    // one press would open this panel and unhook the camera at once.
    if (Input::GetKeyTrigger(VK_TAB))
        m_Open = !m_Open;

    GameObject::Update();
}

void ControlsUI::BindPipeline()
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

void ControlsUI::DrawFlatQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color)
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

void ControlsUI::DrawSprite(ID3D11ShaderResourceView* Texture, float SheetWidth, float SheetHeight,
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

float ControlsUI::DrawPrompt(const InputSprite& Sprite, float X, float Y, float Size)
{
    if (Sprite.Col < 0)
        return X;

    // wide keys (the space bar is 3 tiles) keep their shape
    float width = Size * Sprite.Tiles;

    DrawSprite(m_InputTexture, INPUT_SHEET_WIDTH, INPUT_SHEET_HEIGHT,
        Sprite.Col * INPUT_TILE, Sprite.Row * INPUT_TILE,
        Sprite.Tiles * INPUT_TILE, INPUT_TILE,
        X, Y, width, Size);

    return X + width + 6.0f;
}

void ControlsUI::Draw()
{
    // While something else has paused the game (the start of map reward
    // pick) the panel would just sit on top of it - stay out of the way.
    if (Manager::IsPause())
        return;

    const XMFLOAT4 white = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    const XMFLOAT4 label = XMFLOAT4(0.85f, 0.88f, 0.95f, 1.0f);

    if (!m_Open)
    {
        // closed - just the hint, so F1 is discoverable
        float hintX = 24.0f;
        float hintY = SCREEN_HEIGHT - 48.0f;

        DrawPrompt(KEY_TAB, hintX, hintY, 28.0f);
        Font::Draw("CONTROLS", hintX + 66.0f, hintY + 4.0f, 20.0f,
            XMFLOAT4(1.0f, 1.0f, 1.0f, 0.75f));
        return;
    }

    float panelHeight = 110.0f + s_ControlCount * ROW_HEIGHT;
    float panelX = (SCREEN_WIDTH - PANEL_WIDTH) * 0.5f;
    float panelY = (SCREEN_HEIGHT - panelHeight) * 0.5f;

    DrawFlatQuad(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT,
        XMFLOAT4(0.0f, 0.0f, 0.0f, 0.55f));

    DrawFlatQuad(panelX, panelY, PANEL_WIDTH, panelHeight,
        XMFLOAT4(0.10f, 0.10f, 0.14f, 0.95f));

    // header banner, borrowed from the card sheet so both menus match.
    // 800x77 art, so the height follows the width to keep its shape.
    float bannerWidth = PANEL_WIDTH - 60.0f;
    float bannerHeight = bannerWidth * (BANNER_H / BANNER_W);

    DrawSprite(m_PanelTexture, CARD_SHEET_WIDTH, CARD_SHEET_HEIGHT,
        BANNER_X, BANNER_Y, BANNER_W, BANNER_H,
        panelX + 30.0f, panelY - bannerHeight * 0.5f, bannerWidth, bannerHeight);

    Font::DrawCentered("CONTROLS", panelX + PANEL_WIDTH * 0.5f, panelY - 12.0f, 24.0f, white);

    float rowY = panelY + 70.0f;

    for (int i = 0; i < s_ControlCount; i++)
    {
        const ControlEntry& entry = s_Controls[i];

        float promptX = panelX + 40.0f;
        promptX = DrawPrompt(entry.First, promptX, rowY, PROMPT_SIZE);
        DrawPrompt(entry.Second, promptX, rowY, PROMPT_SIZE);

        Font::Draw(entry.Label, panelX + LABEL_X, rowY + 5.0f, 22.0f, label);

        rowY += ROW_HEIGHT;
    }

    DrawPrompt(KEY_TAB, panelX + 40.0f, panelY + panelHeight - 42.0f, 26.0f);
    Font::Draw("CLOSE", panelX + 100.0f, panelY + panelHeight - 38.0f, 20.0f,
        XMFLOAT4(0.65f, 0.68f, 0.75f, 1.0f));
}
