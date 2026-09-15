#include "main.h"
#include "renderer.h"
#include "PotionSlotUI.h"

#include "PotionBag.h"
#include "Font.h"

// Layout, in screen pixels. The HP and MP bars occupy roughly y 20..95 (see
// the two HPBar::Init calls in Game::Init), so these sit just under them and
// share their left edge.
static const float SLOT_X = 34.0f;
static const float SLOT_Y = 104.0f;
static const float SLOT_SIZE = 46.0f;
static const float SLOT_GAP = 10.0f;
static const float BORDER = 2.0f;

// How far the coloured potion is inset inside its slot, so the frame still
// reads as a frame when the slot is full.
static const float CONTENT_INSET = 7.0f;

static const float KEY_SIZE = 15.0f;

static const XMFLOAT4 FRAME_COLOUR  = XMFLOAT4(0.85f, 0.85f, 0.90f, 0.85f);
static const XMFLOAT4 EMPTY_COLOUR  = XMFLOAT4(0.06f, 0.07f, 0.10f, 0.72f);
// The icons are drawn unmodified, so this is white rather than a colour -
// the bottle carries its own red or blue. It is still here because a missing
// icon file falls back to a flat fill, and a flat WHITE square would say
// nothing about which potion is in the slot.
static const XMFLOAT4 ICON_TINT     = XMFLOAT4(1.00f, 1.00f, 1.00f, 1.00f);
static const XMFLOAT4 HEALTH_COLOUR = XMFLOAT4(0.86f, 0.24f, 0.26f, 0.95f);
static const XMFLOAT4 MANA_COLOUR   = XMFLOAT4(0.30f, 0.52f, 0.95f, 0.95f);

void PotionSlotUI::Init()
{
    m_Layer = 4; // the UI layer, with Score / HPBar / ControlsUI

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

    // The bottles. Authored at 32x32 and supplied at 4x; the slot draws them
    // at whatever CONTENT_INSET leaves, so the extra resolution is just there
    // to survive a bigger slot later.
    //
    // One metadata/image pair each, the way ControlsUI and HPBar load their
    // two textures. Sharing one pair across both calls is the only thing
    // here that differed from the loaders known to work, and it is not worth
    // being the odd one out over.
    TexMetadata healthMetadata;
    ScratchImage healthImage;

    if (SUCCEEDED(LoadFromWICFile(L"asset\\texture\\potion_hp_128.png",
        WIC_FLAGS_NONE, &healthMetadata, healthImage)))
    {
        CreateShaderResourceView(Renderer::GetDevice(), healthImage.GetImages(),
            healthImage.GetImageCount(), healthMetadata, &m_HealthIcon);
    }

    TexMetadata manaMetadata;
    ScratchImage manaImage;

    if (SUCCEEDED(LoadFromWICFile(L"asset\\texture\\potion_mp_128.png",
        WIC_FLAGS_NONE, &manaMetadata, manaImage)))
    {
        CreateShaderResourceView(Renderer::GetDevice(), manaImage.GetImages(),
            manaImage.GetImageCount(), manaMetadata, &m_ManaIcon);
    }

    // These DO assert, like every other texture in the project.
    //
    // They were deliberately left un-asserted at first, on the grounds that a
    // missing icon is not worth killing the build over - the slot falls back
    // to the coloured square it used to draw. That was a mistake: the
    // fallback looks exactly like the icons never having been added, so a
    // file that fails to load reads as "the change is gone" instead of as a
    // missing file. The graceful fallback stays for release builds; this just
    // makes the real cause impossible to miss in a debug one.
    assert(m_HealthIcon);
    assert(m_ManaIcon);
}

void PotionSlotUI::Uninit()
{
    if (m_HealthIcon)   { m_HealthIcon->Release();   m_HealthIcon = nullptr; }
    if (m_ManaIcon)     { m_ManaIcon->Release();     m_ManaIcon = nullptr; }

    if (m_VertexBuffer) { m_VertexBuffer->Release(); m_VertexBuffer = nullptr; }
    if (m_VertexLayout) { m_VertexLayout->Release(); m_VertexLayout = nullptr; }
    if (m_VertexShader) { m_VertexShader->Release(); m_VertexShader = nullptr; }
    if (m_PixelShader)  { m_PixelShader->Release();  m_PixelShader = nullptr; }

    GameObject::Uninit();
}

void PotionSlotUI::MapQuad(float X, float Y, float Width, float Height)
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
}

void PotionSlotUI::DrawFlatQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color)
{
    MapQuad(X, Y, Width, Height);

    MATERIAL material{};
    material.Diffuse = Color;
    material.TextureEnable = false;
    Renderer::SetMaterial(material);

    Renderer::GetDeviceContext()->Draw(4, 0);
}

void PotionSlotUI::DrawSprite(ID3D11ShaderResourceView* Texture,
    float X, float Y, float Width, float Height, const XMFLOAT4& Tint)
{
    // No icon loaded - the slot keeps the coloured square it used to draw,
    // which still says which potion is in it.
    if (Texture == nullptr)
    {
        DrawFlatQuad(X, Y, Width, Height, Tint);
        return;
    }

    MapQuad(X, Y, Width, Height);

    MATERIAL material{};
    material.Diffuse = Tint;
    material.TextureEnable = true;
    Renderer::SetMaterial(material);

    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &Texture);
    Renderer::GetDeviceContext()->Draw(4, 0);
}

void PotionSlotUI::Draw()
{
    // Every quad first, then all the text. Font::Draw binds its own state,
    // so a quad drawn after it would come out with the font's pipeline.
    for (int slot = 0; slot < PotionBag::SlotCount; slot++)
    {
        float x = SLOT_X + slot * (SLOT_SIZE + SLOT_GAP);

        // Frame, then the well punched out of it - two quads rather than a
        // border texture, which keeps this independent of the UI atlas.
        DrawFlatQuad(x, SLOT_Y, SLOT_SIZE, SLOT_SIZE, FRAME_COLOUR);
        DrawFlatQuad(x + BORDER, SLOT_Y + BORDER,
            SLOT_SIZE - BORDER * 2.0f, SLOT_SIZE - BORDER * 2.0f, EMPTY_COLOUR);

        if (!PotionBag::IsFilled(slot))
            continue;

        bool health = PotionBag::GetType(slot) == PotionType::Health;

        DrawSprite(health ? m_HealthIcon : m_ManaIcon,
            x + CONTENT_INSET, SLOT_Y + CONTENT_INSET,
            SLOT_SIZE - CONTENT_INSET * 2.0f, SLOT_SIZE - CONTENT_INSET * 2.0f,
            (health ? m_HealthIcon : m_ManaIcon) != nullptr
                ? ICON_TINT
                : (health ? HEALTH_COLOUR : MANA_COLOUR));
    }

    if (!Font::IsReady())
        return;

    // The key that spends the slot, tucked under it. A dark pass one pixel
    // down-right first - these sit over whatever the map happens to be
    // behind them, the same problem the stat bars have.
    for (int slot = 0; slot < PotionBag::SlotCount; slot++)
    {
        float x = SLOT_X + slot * (SLOT_SIZE + SLOT_GAP);

        char key[2] = { (char)('1' + slot), '\0' };

        float width = Font::Measure(key, KEY_SIZE);
        float keyX = x + (SLOT_SIZE - width) * 0.5f;
        float keyY = SLOT_Y + SLOT_SIZE + 3.0f;

        Font::Draw(key, keyX + 1.0f, keyY + 1.0f, KEY_SIZE, XMFLOAT4(0.0f, 0.0f, 0.0f, 0.55f));
        Font::Draw(key, keyX, keyY, KEY_SIZE, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    }
}
