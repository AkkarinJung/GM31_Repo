#pragma once
#include "GameObject.h"

// Stats panel: what the player currently is, and what the run's rewards have
// made of them. I toggles it, and a hint sits next to the CONTROLS one.
//
// Presentation only - it reads the Player's Stats component and the rewards
// taken so far, and changes neither.
class StatsUI : public GameObject
{
private:
    bool m_Open = false;

    ID3D11Buffer* m_VertexBuffer = nullptr;
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    ID3D11ShaderResourceView* m_InputTexture = nullptr; // key prompts
    ID3D11ShaderResourceView* m_PanelTexture = nullptr; // card sheet, for the banner

    // Font::Draw binds its own buffer and shaders, so every quad here binds
    // again for itself - same as ControlsUI.
    void BindPipeline();
    void DrawFlatQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color);
    void DrawSprite(ID3D11ShaderResourceView* Texture, float SheetWidth, float SheetHeight,
        float SourceX, float SourceY, float SourceWidth, float SourceHeight,
        float X, float Y, float Width, float Height);

    // One "LABEL ....... value" line. Returns the next row's y.
    float DrawStatRow(float PanelX, float Y, const char* Label, const char* Value);
    // A stat that also has a bar, for HP and MP.
    float DrawBarRow(float PanelX, float Y, const char* Label, const char* Value,
        float Fraction, const XMFLOAT4& Color);

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    bool IsOpen() const { return m_Open; }
    void SetOpen(bool Open) { m_Open = Open; }
};
