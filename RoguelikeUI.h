#pragma once
#include "GameObject.h"

// Screen-space card display for the start-of-map reward pick.
//
// Presentation only: it reads the choices out of RoguelikeSystem and draws
// one card per choice. It never applies a reward and never changes the
// selection - the system owns that, and the system is what reads the keys.
// The project has no font, so a card shows its number (the key to press)
// and the reward amount using the same digit spritesheet as Score and
// DamageNumber; the full reward names go to the debug output.
class RoguelikeUI : public GameObject
{
private:
    class RoguelikeSystem* m_System = nullptr;

    ID3D11Buffer* m_VertexBuffer = nullptr;
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;
    ID3D11ShaderResourceView* m_Texture = nullptr;

    void DrawQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color);
    void DrawNumber(int Value, float CenterX, float Y, float DigitSize, const XMFLOAT4& Color);

public:
    void Init() override;
    void Uninit() override;
    void Draw() override;

    void SetSystem(class RoguelikeSystem* System);
};

