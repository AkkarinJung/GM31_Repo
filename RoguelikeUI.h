#pragma once
#include "GameObject.h"

// Screen-space card display for the start-of-map reward pick.
//
// Presentation only: it reads the choices out of RoguelikeSystem and draws
// one card per choice. It never applies a reward and never decides the
// selection - the system owns that. The card art comes from one sprite
// sheet and the labels from the baked Font, so nothing here knows what a
// reward actually does.
class RoguelikeUI : public GameObject
{
private:
    class RoguelikeSystem* m_System = nullptr;

    int m_HoveredIndex = -1; // card under the cursor, -1 when none

    ID3D11Buffer* m_VertexBuffer = nullptr;
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;
    ID3D11ShaderResourceView* m_Texture = nullptr;

    // Font::Draw binds its own buffer and shaders, so anything drawn after
    // text has to bind again - every quad below does it for itself.
    void BindPipeline();
    void DrawFlatQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color);
    void DrawSprite(const struct SpriteRect& Source, float X, float Y, float Width, float Height,
        const XMFLOAT4& Color);
    void DrawWrapped(const char* Text, float CenterX, float Y, float MaxWidth, float Size,
        const XMFLOAT4& Color);

    // One place that decides where a card sits - both the drawing and the
    // mouse hit test read it, so they can never disagree.
    void GetCardRect(int Index, int Count, float& X, float& Y, float& Width, float& Height) const;

public:
    void Init() override;
    void Uninit() override;
    void Draw() override;

    void SetSystem(class RoguelikeSystem* System);

    // Which card is at this screen position, or -1 for none. The layout
    // lives here; the decision to take that card stays in RoguelikeSystem.
    int GetCardIndexAt(float X, float Y) const;
    void SetHoveredIndex(int Index) { m_HoveredIndex = Index; }
};
