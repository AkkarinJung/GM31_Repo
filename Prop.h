#pragma once

#include "GameObject.h"

// A piece of scenery. It loads a model, stands where it is put, and draws -
// no collision and no per-frame work, which is what lets the background be
// this many objects.
//
// Everything in asset\model\Enviroment comes from one pack built at a hundred
// times game scale, so a single uniform scale suits all of them and keeps a
// tree towering over a flower the way the artist made them.
class Prop : public GameObject
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

public:
    void Init() override;
    void Uninit() override;
    void Draw() override;

    // Called after the object exists, the way Polygon2D is set up. SizeScale
    // multiplies the pack scale, for variety between copies of one model.
    void Load(const char* FileName, float SizeScale = 1.0f);
};
