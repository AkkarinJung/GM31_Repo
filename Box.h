#pragma once
#include "GameObject.h"

class Box : public GameObject
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

    // Maps the model as the artist built it onto the crate shape the
    // collision assumes. See Init.
    Vector3 m_FitScale{ 1.0f, 1.0f, 1.0f };
    Vector3 m_FitOffset{ 0.0f, 0.0f, 0.0f };

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

};


