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

	// Which stage the run is on. Static because clearing a stage rebuilds
	// the whole scene - the Game object itself does not survive, so the
	// progress cannot live in a normal member.
	static int s_Stage;

	// The clear check runs every frame while the next scene fades in, so
	// this makes it fire exactly once.
	bool m_Cleared = false;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	RoguelikeSystem& GetRoguelike() { return m_Roguelike; }

	static int GetStageIndex() { return s_Stage; }

	// Call when a new run starts (from the title), not between stages.
	static void ResetProgress();
};