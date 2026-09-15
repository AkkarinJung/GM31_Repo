#pragma once
#include"GameObject.h"
			

class Particle :public GameObject
{
private:

	ID3D11Buffer* m_VertexBuffer;
	ID3D11InputLayout* m_VertexLayout;
	ID3D11VertexShader* m_VertexShader;
	ID3D11PixelShader* m_PixelShader;

	ID3D11ShaderResourceView* m_LaunchTexture;
	ID3D11ShaderResourceView* m_ToraTexture;
	ID3D11ShaderResourceView* m_DustTexture;

	enum PARTICLE_TYPE
	{
		PARTICLE_LAUNCH,
		PARTICLE_EXPLOSION,
		PARTICLE_DUST,      // jump dust - its own texture, see Draw
	};
	// 変数
	struct PARTICLE
	{
		bool	Enable;
		PARTICLE_TYPE Type;
		Vector3	Position;
		Vector3	Velocity;
		int		Life;

		// Per particle now, not per draw call. Every particle used to be the
		// same size and the same red, because the size came off the emitter's
		// own m_Scale and the colour was one MATERIAL set before the loop -
		// so jump dust would have been a burst of big red fireballs.
		float	 Size;
		XMFLOAT4 Colour;
	};

	static const int PARTICLE_MAX = 50; // パーティクルの数
	PARTICLE	m_Particle[PARTICLE_MAX];

	void CreateExplosion(Vector3 pos);

	// Finds a free slot and fills in the common fields. Returns nullptr when
	// the pool is full, which is a normal thing to be - a burst that cannot
	// fit is dropped rather than stealing a particle that is still playing.
	PARTICLE* Spawn(const Vector3& Position, const Vector3& Velocity, int Life,
		float Size, const XMFLOAT4& Colour, PARTICLE_TYPE Type);

public:
	void Init();
	void Uninit();
	void Update();
	void Draw();

	// A puff of dust kicked out sideways from a pair of feet. Position is the
	// feet, not the middle - Player's position already is.
	void JumpDust(const Vector3& Position);

	// The firework burst, as a call rather than as something the emitter does
	// to itself when SPACE is pressed.
	void Burst(const Vector3& Position) { CreateExplosion(Position); }
};

