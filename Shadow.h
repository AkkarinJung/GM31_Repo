#pragma once

#include"GameObject.h"

class Shadow :public GameObject
{
private:
	// m_Position / m_Rotation / m_Scale used to be redeclared here. They HID
	// GameObject's - which is where SetPosition writes and GetMatrx reads - so
	// these three were never read by anything. Tree.h has the same three
	// commented out for the same reason.
	//
	// Whoever owns this shadow places it from its own Draw(), NOT its Update():
	// Update is skipped while the game is paused, and the reward card screen
	// pauses at the end of Game::Init, before any Update has run. A shadow
	// placed from an Update was therefore still on the world origin - right
	// under the player - for the whole card screen.

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
};
