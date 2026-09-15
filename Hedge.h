#pragma once

#include "GameObject.h"

// A hedge segment, used as the edge of the map. The model is what the player
// sees; the solid it hands Collision is what actually stops them.
//
// The solid is measured off the loaded model rather than typed in, so
// re-exporting the hedge at another size, or changing its scale, keeps the
// invisible wall matching the visible one.
class Hedge : public GameObject
{
private:
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    // Toon ramp, sampled at t1 by toonPS - the same shader the enemies use.
    ID3D11ShaderResourceView* m_RampTexture = nullptr;

    // x = ramp row, y = edge start, z = edge darkness, w = edge softness.
    // A paler, thinner rim than Enemy's: an outline that reads well on one
    // character turns a field full of bushes into noise.
    XMFLOAT4 m_Parameter{ 0.125f, -0.25f, 0.55f, 0.25f };

    // Half extents of the model itself, before this object's scale.
    Vector3 m_ModelHalfSize{ 1.0f, 1.0f, 1.0f };

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    // Half extents of the collision box in world units. Worked out on demand
    // rather than in Init, because the scene sets rotation and scale after
    // the object is created and both change the answer.
    Vector3 GetSolidHalfSize() const;
};
