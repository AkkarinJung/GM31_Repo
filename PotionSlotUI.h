#pragma once
#include "GameObject.h"

// The two potion slots, drawn under the HP and MP bars.
//
// Presentation only - it reads PotionBag and changes nothing. Drinking is
// Player::Update's job, the same split HPBar has with Stats: the bar shows
// the number, it never spends it.
class PotionSlotUI : public GameObject
{
private:
    ID3D11Buffer* m_VertexBuffer = nullptr;
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    // Font::Draw binds its own buffer and shaders, so anything drawn after
    // text has to bind again - the quad helper does it for itself.
    void DrawFlatQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color);

public:
    void Init() override;
    void Uninit() override;
    void Draw() override;
};
