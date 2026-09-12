#pragma once
#include "GameObject.h"

// Shows which stage the player is on, and says so when the stage is clear.
//
// Reads nothing but "how many enemies are left" and the stage table, so it
// stays out of the way of the stage logic in Game - it only reports.
class StageUI : public GameObject
{
private:
    float m_Timer = 0.0f; // seconds since this stage started

    ID3D11Buffer* m_VertexBuffer = nullptr;
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;
    ID3D11ShaderResourceView* m_Texture = nullptr; // card sheet, for the banner

    // Font::Draw binds its own buffer and shaders, so anything drawn after
    // text has to bind again - the banner does it for itself.
    void DrawBanner(float X, float Y, float Width, float Height, const XMFLOAT4& Color);

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;
};
