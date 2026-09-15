#pragma once
#include "GameObject.h"
class Score :public GameObject
{
private:
	int m_Score;

	// Clearing a stage rebuilds the whole scene, so m_Score above only ever
	// holds the CURRENT stage's kills - it is gone the moment the next map
	// loads. The run total has to outlive that, which is what the end screen
	// reports.
	static int s_RunTotal;
	ID3D11Buffer* m_VertexBuffer;
	ID3D11InputLayout* m_VertexLayout;
	ID3D11VertexShader* m_VertexShader;
	ID3D11PixelShader* m_PixelShader;

	ID3D11ShaderResourceView* m_Texture;
public:
	void Init();
	void Uninit();
	void Update();
	void Draw();

	void Add(int Score) { m_Score += Score; s_RunTotal += Score; }

	static int GetRunTotal() { return s_RunTotal; }
	static void ResetRun() { s_RunTotal = 0; } // a new run, not a new stage
};

