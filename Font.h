#pragma once

// Bitmap font baked from a .ttf at startup.
//
// Windows already parses TrueType, so no font library is needed: GDI draws
// every glyph once into a memory bitmap, that bitmap becomes one texture,
// and drawing text is then the same textured quads the rest of the UI
// already uses. All the GDI work happens in Init - nothing per frame.
//
//   Font::Init(L"asset\\font\\kenvector_future.ttf", L"KenVector Future", 48);
//   Font::DrawCentered("+20 Max HP", 640.0f, 300.0f, 24.0f, color);
class Font
{
private:
    struct Glyph
    {
        float U = 0.0f, V = 0.0f;             // top left in the atlas, 0-1
        float UWidth = 0.0f, VHeight = 0.0f;  // size in the atlas, 0-1
        float Width = 0.0f, Height = 0.0f;    // size in pixels at the baked height
        float Advance = 0.0f;                 // pen step to the next character
    };

    static const int FIRST_CHAR = 32;  // space
    static const int LAST_CHAR = 126;  // ~
    static const int CHAR_COUNT = LAST_CHAR - FIRST_CHAR + 1;

    static Glyph m_Glyphs[CHAR_COUNT];
    static float m_BakedHeight;

    static ID3D11Buffer* m_VertexBuffer;
    static ID3D11InputLayout* m_VertexLayout;
    static ID3D11VertexShader* m_VertexShader;
    static ID3D11PixelShader* m_PixelShader;
    static ID3D11ShaderResourceView* m_Texture;

    static void Bake(const WCHAR* FaceName, int PixelHeight);

public:
    // FontFile is the .ttf on disk, FaceName the family name inside it
    // (kenvector_future.ttf -> L"KenVector Future"). PixelHeight is what the
    // glyphs are rasterized at; Draw scales from there, so bake it at least
    // as large as the biggest text on screen.
    static void Init(const WCHAR* FontFile, const WCHAR* FaceName, int PixelHeight = 48);
    static void Uninit();

    // X, Y is the top left of the text. Size is the line height in pixels.
    static void Draw(const char* Text, float X, float Y, float Size, const XMFLOAT4& Color);
    static void DrawCentered(const char* Text, float CenterX, float Y, float Size, const XMFLOAT4& Color);

    static float Measure(const char* Text, float Size);
    static bool IsReady() { return m_Texture != nullptr; }
};
