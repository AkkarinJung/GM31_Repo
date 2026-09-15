#pragma once

#include"GameObject.h"

class Tree :public GameObject
{
private:
	//Vector3 m_Position{ 0.0f,0.0f,0.0f };
	//Vector3 m_Rotation{ 0.0f,0.0f,0.0f };
	//Vector3 m_Scale{ 1.0f,1.0f,1.0f };

	ID3D11Buffer* m_VertexBuffer;
	ID3D11InputLayout* m_VertexLayout;
	ID3D11VertexShader* m_VertexShader;
	ID3D11PixelShader* m_PixelShader;

	static ID3D11ShaderResourceView* m_Texture;

	GameObject* m_Shadow;

public:
	// Trees do not get a shadow unless asked. It used to be the other way
	// round - Init made one and the caller destroyed it again - and the only
	// caller there has ever been (Game's BuildTreeLine) threw every one of
	// them away. That was hundreds of Shadow objects built and torn down per
	// stage load, each one decoding shadow.png and re-reading both compiled
	// shaders off disk, for nothing. The maps are wider now, so it was about
	// to get worse.
	//
	// Call this for a tree close enough to the player for a shadow to read.
	void EnableShadow();

	void Init();
	void Uninit();
	void Update();
	void Draw();
};
