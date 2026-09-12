#pragma once
#include "GameObject.h"

// Controls panel: one row per binding, drawn from the input prompt sheet
// with a label next to it. F1 toggles it, and a small hint sits in the
// corner so the player knows F1 does something.
//
// Presentation only - it reads no gameplay state and changes none. The
// binding list at the top of ControlsUI.cpp is the whole content, so adding
// a control is one line there.
class ControlsUI : public GameObject
{
private:
    bool m_Open = false;

    ID3D11Buffer* m_VertexBuffer = nullptr;
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    ID3D11ShaderResourceView* m_InputTexture = nullptr; // key / mouse prompts
    ID3D11ShaderResourceView* m_PanelTexture = nullptr; // card sheet, for the banner

    // Font::Draw binds its own buffer and shaders, so anything drawn after
    // text has to bind again - every quad below does it for itself.
    void BindPipeline();
    void DrawFlatQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color);
    void DrawSprite(ID3D11ShaderResourceView* Texture, float SheetWidth, float SheetHeight,
        float SourceX, float SourceY, float SourceWidth, float SourceHeight,
        float X, float Y, float Width, float Height);

    // Returns where the next prompt should start, so a binding with two
    // keys (A and D) lays them out in a row without the caller doing maths.
    float DrawPrompt(const struct InputSprite& Sprite, float X, float Y, float Size);

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    void SetOpen(bool Open) { m_Open = Open; }
    bool IsOpen() const { return m_Open; }
};
