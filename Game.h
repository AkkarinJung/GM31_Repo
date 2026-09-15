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

	// Set only when the final stage is cleared. s_Stage is already pointing
	// at the next stage by then, so nothing can tell "last stage cleared"
	// from "next stage starting" by looking at the index alone.
	static bool s_RunComplete;

	// HP the player finished the previous stage on. Clearing a stage rebuilds
	// the scene, so the Player is a new object back at full HP - this is what
	// carries the damage taken across the gap. -1 means "nothing to restore",
	// which is the first stage of a run.
	static int s_CarriedHP;
	static const int NoCarriedHP = -1;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;


	// The playable span in x. The hedges that wall the map in stand on these,
	// and Camera stops short of them - walk the camera all the way to the
	// edge and the near end of a hedge lands inside the near plane, which is
	// what lets you see through it.
	//
	// Set from the stage table at the top of Init(), not fixed: every stage
	// is a different size. Anything that reads them runs after that.
	static float MapLeft;
	static float MapRight;

	static int GetStageIndex() { return s_Stage; }
	static bool IsRunComplete() { return s_RunComplete; }

	// Call when a new run starts (from the title), not between stages.
	static void ResetProgress();
};