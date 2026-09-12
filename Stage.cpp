#include "main.h"
#include "Stage.h"

#define COUNT_OF(Array) ((int)(sizeof(Array) / sizeof(Array[0])))

// Everything is laid out along X at Z = 0 - the game is 2.5D and the player
// is pinned to that plane, so a spawn only really chooses "how far along"
// and "how high".
//
// Two rules the layouts have to respect, both from Player.cpp:
//
//  * A crate is only climbable while Scale.y is under the jump apex (1.877
//    with JumpPower 20 and gravity 98). Player.cpp only snaps the player on
//    top when y rises above Position.y + Scale.y; anything taller is a solid
//    wall that seals off everything behind it - and a stage whose last enemy
//    sits behind a wall can never be cleared.
//
//  * A crate covers x from Position.x - Scale.x to Position.x + Scale.x.
//    Enemies have no crate collision, so one spawned inside that span starts
//    embedded in it.

// One crate to climb (x 3 to 7, top at 2.0), both enemies past it.
static const EnemySpawn s_Stage1Enemies[] =
{
    { EnemyType::Walker, {  9.0f, 0.0f, 0.0f } },
    { EnemyType::Walker, { 12.0f, 0.0f, 0.0f } },
};

static const BoxSpawn s_Stage1Boxes[] =
{
    { { 5.0f, 0.0f, 0.0f }, { 2.0f, 1.0f, 2.0f } },
};

// Enemies on both sides, and the turret is past the second crate - which is
// why that crate has to stay climbable.
static const EnemySpawn s_Stage2Enemies[] =
{
    { EnemyType::Walker,    {  7.5f, 0.0f, 0.0f } },
    { EnemyType::Walker,    { 15.0f, 0.0f, 0.0f } },
    { EnemyType::Patroller, { -6.0f, 0.0f, 0.0f } },
    { EnemyType::Turret,    { 17.0f, 0.0f, 0.0f } },
};

static const BoxSpawn s_Stage2Boxes[] =
{
    { {  4.0f, 0.0f, 0.0f }, { 2.0f, 1.00f, 2.0f } }, // x 2 to 6,  top 2.0
    { { 11.0f, 0.0f, 0.0f }, { 2.0f, 1.25f, 2.0f } }, // x 9 to 13, top 2.5
};

// Flyers hover 2.5 above the player, which is outside the sword's 2.0 reach
// from the ground - they are meant to be hit at the top of a jump.
static const EnemySpawn s_Stage3Enemies[] =
{
    { EnemyType::Walker, {  6.0f, 0.0f, 0.0f } },
    { EnemyType::Walker, { 13.0f, 0.0f, 0.0f } },
    { EnemyType::Walker, { -9.0f, 0.0f, 0.0f } },
    { EnemyType::Flyer,  {  4.0f, 3.0f, 0.0f } },
    { EnemyType::Flyer,  { -4.0f, 3.0f, 0.0f } },
};

static const BoxSpawn s_Stage3Boxes[] =
{
    { {  3.0f, 0.0f, 0.0f }, { 2.0f, 0.75f, 2.0f } }, // x  1 to  5, top 1.5
    { {  9.0f, 0.0f, 0.0f }, { 2.0f, 1.25f, 2.0f } }, // x  7 to 11, top 2.5
    { { -5.0f, 0.0f, 0.0f }, { 2.0f, 1.00f, 2.0f } }, // x -7 to -3, top 2.0
};

// The run, in order. Add a stage by adding a line here.
static const StageData s_Stages[] =
{
    { "STAGE 1", s_Stage1Enemies, COUNT_OF(s_Stage1Enemies), s_Stage1Boxes, COUNT_OF(s_Stage1Boxes) },
    { "STAGE 2", s_Stage2Enemies, COUNT_OF(s_Stage2Enemies), s_Stage2Boxes, COUNT_OF(s_Stage2Boxes) },
    { "STAGE 3", s_Stage3Enemies, COUNT_OF(s_Stage3Enemies), s_Stage3Boxes, COUNT_OF(s_Stage3Boxes) },
};

int GetStageCount()
{
    return COUNT_OF(s_Stages);
}

const StageData& GetStageData(int Index)
{
    if (Index < 0)
        Index = 0;

    if (Index >= GetStageCount())
        Index = GetStageCount() - 1;

    return s_Stages[Index];
}
