#include "main.h"
#include "Stage.h"

#define COUNT_OF(Array) ((int)(sizeof(Array) / sizeof(Array[0])))

// Everything is laid out along X at Z = 0 - the game is 2.5D and the player
// is pinned to that plane, so a spawn only really chooses "how far along"
// and "how high".

static const EnemySpawn s_Stage1Enemies[] =
{
    { EnemyType::Walker, {  5.0f, 0.0f, 0.0f } },
    { EnemyType::Walker, {  7.0f, 0.0f, 0.0f } },
};

static const BoxSpawn s_Stage1Boxes[] =
{
    { { 5.0f, 0.0f, 0.0f }, { 2.0f, 1.0f, 2.0f } },
};

static const EnemySpawn s_Stage2Enemies[] =
{
    { EnemyType::Walker,    {  6.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    {  9.0f, 0.0f, 0.0f } },
    { EnemyType::Patroller, { -6.0f, 0.0f, 0.0f } },
    { EnemyType::Turret,    { 13.0f, 0.0f, 0.0f } },
};

static const BoxSpawn s_Stage2Boxes[] =
{
    { {  4.0f, 0.0f, 0.0f }, { 2.0f, 1.0f, 2.0f } },
    { { 11.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f } },
};

static const EnemySpawn s_Stage3Enemies[] =
{
    { EnemyType::Walker, {  5.0f, 0.0f, 0.0f } },
    { EnemyType::Walker, {  8.0f, 0.0f, 0.0f } },
    { EnemyType::Walker, { -7.0f, 0.0f, 0.0f } },
    { EnemyType::Flyer,  {  4.0f, 3.0f, 0.0f } },
    { EnemyType::Flyer,  { -4.0f, 3.0f, 0.0f } },
};

static const BoxSpawn s_Stage3Boxes[] =
{
    { {  3.0f, 0.0f, 0.0f }, { 2.0f, 1.0f, 2.0f } },
    { {  9.0f, 0.0f, 0.0f }, { 2.0f, 2.0f, 2.0f } },
    { { -5.0f, 0.0f, 0.0f }, { 2.0f, 1.5f, 2.0f } },
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
