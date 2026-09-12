#pragma once

#include"GameObject.h"
class SkyDome :public GameObject
{
private:
	ID3D11Buffer* m_VertexBuffer;
	ID3D11InputLayout* m_VertexLayout;
	ID3D11VertexShader* m_VertexShader;
	ID3D11PixelShader* m_PixelShader;

	ID3D11ShaderResourceView* m_Texture;

	float m_RotationSpeed = 0.015f;	// ƒ‰ƒWƒAƒ“–ˆ•bAˆêü‚¨‚æ‚»7•ª

public:
	void Init();
	void Uninit();
	void Update();
	void Draw();
};

