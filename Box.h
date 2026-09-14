#pragma once
#include "GameObject.h"

class Box : public GameObject
{
private:


    ID3D11InputLayout* m_VertexLayout;
    ID3D11VertexShader* m_VertexShader;
    ID3D11PixelShader* m_PixelShader;

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


