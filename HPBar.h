#pragma once
#include "GameObject.h"

// Screen-space HP bar: a static background quad plus a fill quad that
// shrinks left-to-right based on Target's Stats component. Reusable for
// Player, Enemy, or anything else with a Stats component - just pass a
// different Target and fill texture.
class HPBar : public GameObject
{
private:
    float m_X = 0.0f, m_Y = 0.0f, m_Width = 0.0f, m_Height = 0.0f;

    class GameObject* m_Target = nullptr;

    ID3D11Buffer* m_BackgroundVertexBuffer = nullptr;
    ID3D11Buffer* m_FillVertexBuffer = nullptr;

    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    ID3D11ShaderResourceView* m_BackgroundTexture = nullptr;
    ID3D11ShaderResourceView* m_FillTexture = nullptr;

public:
    void Init() override {}
    void Init(float X, float Y, float Width, float Height, GameObject* Target, const WCHAR* FillTextureName);
    void Uninit() override;
    void Update() override;
    void Draw() override;
};
