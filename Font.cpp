#include "main.h"
#include "renderer.h"
#include "Font.h"

#define ATLAS_SIZE (1024)

Font::Glyph Font::m_Glyphs[Font::CHAR_COUNT];
float Font::m_BakedHeight = 0.0f;

ID3D11Buffer* Font::m_VertexBuffer = nullptr;
ID3D11InputLayout* Font::m_VertexLayout = nullptr;
ID3D11VertexShader* Font::m_VertexShader = nullptr;
ID3D11PixelShader* Font::m_PixelShader = nullptr;
ID3D11ShaderResourceView* Font::m_Texture = nullptr;

static const WCHAR* s_FontFile = nullptr; // kept so Uninit can release it


void Font::Init(const WCHAR* FontFile, const WCHAR* FaceName, int PixelHeight)
{
    // FR_PRIVATE loads the file for this process only - nothing is installed
    // on the player's machine and no admin rights are needed.
    AddFontResourceExW(FontFile, FR_PRIVATE, nullptr);
    s_FontFile = FontFile;

    VERTEX_3D vertex[4]{};

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(VERTEX_3D) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertex; // initial data, remapped per character in Draw()

    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\unlitTextureVS.cso");
    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\unlitTexturePS.cso");

    Bake(FaceName, PixelHeight);
}

void Font::Bake(const WCHAR* FaceName, int PixelHeight)
{
    HDC screenDC = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screenDC);
    ReleaseDC(nullptr, screenDC);

    // Top-down 32 bit bitmap, so its rows can be handed to D3D as they are.
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = ATLAS_SIZE;
    info.bmiHeader.biHeight = -ATLAS_SIZE;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBitmap = SelectObject(dc, bitmap);

    // A negative height asks for a size in pixels rather than in points.
    HFONT font = CreateFontW(-PixelHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        FF_DONTCARE, FaceName);
    HGDIOBJ oldFont = SelectObject(dc, font);

    // GDI never writes an alpha channel, so the glyphs are drawn white on
    // black and that brightness becomes the alpha further down.
    RECT full = { 0, 0, ATLAS_SIZE, ATLAS_SIZE };
    FillRect(dc, &full, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));

    TEXTMETRICW metrics{};
    GetTextMetricsW(dc, &metrics);
    m_BakedHeight = (float)metrics.tmHeight;

    int penX = 1;
    int penY = 1;

    for (int c = FIRST_CHAR; c <= LAST_CHAR; c++)
    {
        WCHAR character = (WCHAR)c;

        SIZE size{};
        GetTextExtentPoint32W(dc, &character, 1, &size);

        if (penX + size.cx + 2 > ATLAS_SIZE)
        {
            penX = 1;
            penY += metrics.tmHeight + 2;
        }

        if (penY + metrics.tmHeight + 2 > ATLAS_SIZE)
            break; // atlas full - raise ATLAS_SIZE or bake smaller

        TextOutW(dc, penX, penY, &character, 1);

        Glyph& glyph = m_Glyphs[c - FIRST_CHAR];
        glyph.U = penX / (float)ATLAS_SIZE;
        glyph.V = penY / (float)ATLAS_SIZE;
        glyph.UWidth = size.cx / (float)ATLAS_SIZE;
        glyph.VHeight = metrics.tmHeight / (float)ATLAS_SIZE;
        glyph.Width = (float)size.cx;
        glyph.Height = (float)metrics.tmHeight;
        glyph.Advance = (float)size.cx;

        penX += size.cx + 2;
    }

    // brightness -> alpha, color -> white, so Material.Diffuse tints the text
    unsigned char* pixel = (unsigned char*)bits;
    for (int i = 0; i < ATLAS_SIZE * ATLAS_SIZE; i++)
    {
        unsigned char luminance = pixel[0];
        if (pixel[1] > luminance) luminance = pixel[1];
        if (pixel[2] > luminance) luminance = pixel[2];

        pixel[0] = 255;
        pixel[1] = 255;
        pixel[2] = 255;
        pixel[3] = luminance;

        pixel += 4;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = ATLAS_SIZE;
    desc.Height = ATLAS_SIZE;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA data{};
    data.pSysMem = bits;
    data.SysMemPitch = ATLAS_SIZE * 4;

    ID3D11Texture2D* texture = nullptr;
    Renderer::GetDevice()->CreateTexture2D(&desc, &data, &texture);

    if (texture != nullptr)
    {
        Renderer::GetDevice()->CreateShaderResourceView(texture, nullptr, &m_Texture);
        texture->Release();
    }

    SelectObject(dc, oldFont);
    DeleteObject(font);
    SelectObject(dc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
}

void Font::Uninit()
{
    if (m_Texture != nullptr) { m_Texture->Release(); m_Texture = nullptr; }
    if (m_VertexBuffer != nullptr) { m_VertexBuffer->Release(); m_VertexBuffer = nullptr; }
    if (m_VertexLayout != nullptr) { m_VertexLayout->Release(); m_VertexLayout = nullptr; }
    if (m_VertexShader != nullptr) { m_VertexShader->Release(); m_VertexShader = nullptr; }
    if (m_PixelShader != nullptr) { m_PixelShader->Release(); m_PixelShader = nullptr; }

    if (s_FontFile != nullptr)
    {
        RemoveFontResourceExW(s_FontFile, FR_PRIVATE, nullptr);
        s_FontFile = nullptr;
    }
}

float Font::Measure(const char* Text, float Size)
{
    if (Text == nullptr || m_BakedHeight <= 0.0f)
        return 0.0f;

    float scale = Size / m_BakedHeight;
    float width = 0.0f;

    for (const char* p = Text; *p != '\0'; p++)
    {
        unsigned char c = (unsigned char)*p;
        if (c < FIRST_CHAR || c > LAST_CHAR)
            continue;

        width += m_Glyphs[c - FIRST_CHAR].Advance * scale;
    }

    return width;
}

void Font::DrawCentered(const char* Text, float CenterX, float Y, float Size, const XMFLOAT4& Color)
{
    Draw(Text, CenterX - Measure(Text, Size) * 0.5f, Y, Size, Color);
}

void Font::Draw(const char* Text, float X, float Y, float Size, const XMFLOAT4& Color)
{
    if (Text == nullptr || m_Texture == nullptr || m_BakedHeight <= 0.0f)
        return;

    float scale = Size / m_BakedHeight;

    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    Renderer::SetWorldViewProjection2D();
    Renderer::SetWorldMatrix(XMMatrixIdentity());

    MATERIAL material{};
    material.Diffuse = Color;
    material.TextureEnable = true;
    Renderer::SetMaterial(material);

    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    float penX = X;

    for (const char* p = Text; *p != '\0'; p++)
    {
        unsigned char c = (unsigned char)*p;
        if (c < FIRST_CHAR || c > LAST_CHAR)
            continue;

        const Glyph& glyph = m_Glyphs[c - FIRST_CHAR];

        float width = glyph.Width * scale;
        float height = glyph.Height * scale;

        D3D11_MAPPED_SUBRESOURCE msr{};
        if (SUCCEEDED(Renderer::GetDeviceContext()->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
        {
            VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;

            vertex[0].Position = XMFLOAT3(penX, Y, 0.0f);
            vertex[0].Normal = XMFLOAT3(0, 0, 0);
            vertex[0].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[0].TexCoord = XMFLOAT2(glyph.U, glyph.V);

            vertex[1].Position = XMFLOAT3(penX + width, Y, 0.0f);
            vertex[1].Normal = XMFLOAT3(0, 0, 0);
            vertex[1].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[1].TexCoord = XMFLOAT2(glyph.U + glyph.UWidth, glyph.V);

            vertex[2].Position = XMFLOAT3(penX, Y + height, 0.0f);
            vertex[2].Normal = XMFLOAT3(0, 0, 0);
            vertex[2].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[2].TexCoord = XMFLOAT2(glyph.U, glyph.V + glyph.VHeight);

            vertex[3].Position = XMFLOAT3(penX + width, Y + height, 0.0f);
            vertex[3].Normal = XMFLOAT3(0, 0, 0);
            vertex[3].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[3].TexCoord = XMFLOAT2(glyph.U + glyph.UWidth, glyph.V + glyph.VHeight);

            Renderer::GetDeviceContext()->Unmap(m_VertexBuffer, 0);
        }

        Renderer::GetDeviceContext()->Draw(4, 0);

        penX += glyph.Advance * scale;
    }
}
