#pragma once
#include "GameObject.h"

// A small HP bar floating over every enemy.
//
// One object draws all of them: it walks the live enemies each frame rather
// than keeping a bar per enemy, so an enemy that dies simply stops being
// drawn - there is no pointer to go stale and nothing to clean up when the
// stage rebuilds. Add it once in the scene, like the other HUD pieces.
class EnemyHPBar : public GameObject
{
private:
    ID3D11Buffer* m_VertexBuffer = nullptr;
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    void DrawQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color);

public:
    void Init() override;
    void Uninit() override;
    void Draw() override;
};
