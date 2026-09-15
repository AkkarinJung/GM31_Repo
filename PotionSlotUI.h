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

    // One per potion type, so a full slot shows the bottle rather than a
    // coloured square standing in for it.
    ID3D11ShaderResourceView* m_HealthIcon = nullptr;
    ID3D11ShaderResourceView* m_ManaIcon = nullptr;

    // Font::Draw binds its own buffer and shaders, so anything drawn after
    // text has to bind again - both quad helpers do it for themselves.
    // Binds the pipeline and writes the rectangle into the vertex buffer.
    // Does NOT draw - both helpers below decide the material first, because
    // drawing an untextured pass under a transparent icon would show a solid
    // block through every part of the bottle that is meant to be see-through.
    void MapQuad(float X, float Y, float Width, float Height);

    void DrawFlatQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color);

    // Same quad, with a texture on it. Null Texture falls back to a flat
    // fill, so a missing icon file leaves the slot readable instead of
    // leaving it blank.
    void DrawSprite(ID3D11ShaderResourceView* Texture,
        float X, float Y, float Width, float Height, const XMFLOAT4& Tint);

public:
    void Init() override;
    void Uninit() override;
    void Draw() override;
};
