#pragma once
#include "Scene.h"
#include "RoguelikeSystem.h"

class Game : public Scene
{
private:
	// Start-of-map reward pick. Owned by the scene, so it is created and
	// destroyed with the map - a new map always gets a fresh pick, and the
	// same map can never run one twice.
	RoguelikeSystem m_Roguelike;
public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	RoguelikeSystem& GetRoguelike() { return m_Roguelike; }
};

