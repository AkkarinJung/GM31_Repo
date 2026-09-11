#pragma once
#include "GameObject.h"

// Floating number popup that appears at a world position (e.g. above a
// character's head), drifts upward on screen, and destroys itself after a
// short lifetime. Not tied to damage specifically - pass a color and
// ShowSign=true to use it for heals, buffs, etc. Reuses the digit
// spritesheet from Score; +/- are drawn as flat colored bars since the
// spritesheet has no sign glyphs.
class DamageNumber : public GameObject
{
private:
    int m_Value = 0;
    bool m_ShowSign = false;
    XMFLOAT4 m_Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    float m_Timer = 0.0f;
    const float m_Lifetime = 0.8f;
    const float m_RiseSpeed = 40.0f; // screen pixels/sec

    ID3D11Buffer* m_VertexBuffer = nullptr;
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;
    ID3D11ShaderResourceView* m_Texture = nullptr;

    void DrawFlatQuad(float X, float Y, float Width, float Height);

public:
    void Init() override {}
    void Init(const Vector3& WorldPosition, int Value, bool ShowSign = false,
        const XMFLOAT4& Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    void Uninit() override;
    void Update() override;
    void Draw() override;
};