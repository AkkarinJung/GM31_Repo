#pragma once
#include "Vector3.h"

// What one stage contains.
//
// A stage is plain data - what to spawn and where. Game::Init builds the map
// from the entry for the current stage, so adding a stage means adding one
// entry to the table in Stage.cpp: no new scene, no new code path, nothing
// else to touch.

enum class EnemyType
{
    Patroller, // walks a fixed beat, ignores the player
    Walker,    // patrols, then chases and melees once it spots the player
    Turret,    // never moves, attacks whatever comes close
    Flyer,     // hovers and drifts toward the player through the air
};

struct EnemySpawn
{
    EnemyType Type;
    Vector3 Position;
};

// Boxes double as platforms, so they are what makes one stage's shape
// different from another's.
struct BoxSpawn
{
    Vector3 Position;
    Vector3 Scale;
};

struct StageData
{
    const char* Name;
    const EnemySpawn* Enemies;
    int EnemyCount;
    const BoxSpawn* Boxes;
    int BoxCount;
};

const StageData& GetStageData(int Index); // clamped, so a bad index cannot crash
int GetStageCount();
