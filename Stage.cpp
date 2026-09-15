#include "main.h"
#include "Stage.h"

#define COUNT_OF(Array) ((int)(sizeof(Array) / sizeof(Array[0])))

// Everything is laid out along X at Z = 0 - the game is 2.5D and the player
// is pinned to that plane, so a spawn only really chooses "how far along"
// and "how high".
//
// Rules every layout here respects:
//
//  * A crate is only climbable while its TOP is under the jump apex. The top
//    is Scale.y * 2 - crates stand on their position - and the apex is 2.98
//    with JumpPower 25 and gravity 98, so Scale.y never goes above 1.25 (top
//    2.5). A taller crate is a wall that seals off everything behind it, and
//    a stage whose last enemy sits behind a wall can never be cleared.
//
//  * A crate covers x from Position.x - Scale.x to Position.x + Scale.x. An
//    enemy spawned inside that span starts embedded in it, so every ground
//    spawn below sits in a gap.
//
//  * A FLIER spawns with y above the ground (3.0 below) - the AI drives it to
//    its own hover height from there, but starting it in the air keeps it
//    from being shoved about by the ground crowd on its first frame. A crate
//    underneath one is harmless: it flies over everything.
//
//  * A TURRET cannot move at all, so where it is placed is the whole of its
//    behaviour. It is the only type that throws a wave, and its range is 9,
//    so it is put where the player has ground to cross in front of it.
//
//  * An enemy spawned on a crate has y = that crate's top, and x inside the
//    crate's span. Only the wide crates carry one - a walker patrols four
//    units each way, so a 4-wide crate would just walk it off the edge.
//
//  * The player starts at x = 0. Nothing spawns within about four units of
//    that, so the stage does not open with a swing already coming.
//
// Sizes used below, so the vocabulary stays consistent between stages:
//
//    step      Scale {2, 0.75, 2}   4 wide, top 1.5   an easy hop
//    block     Scale {2, 1.00, 2}   4 wide, top 2.0   a real step up
//    tall      Scale {2, 1.25, 2}   4 wide, top 2.5   the highest climbable
//    platform  Scale {4, 1.00, 2}   8 wide, top 2.0   room to stand and fight
//    long      Scale {5, 1.25, 2}  10 wide, top 2.5   high ground, holds a fight
//
// Breakable crates (s_StageNCrates) are a different thing from the platforms
// above, despite the shared name - they are one unit across, they are solid
// until the player breaks them, and each one rolls the drop table in
// Crate::Break. Placement rules, all checked against the layouts above:
//
//  * A breakable covers x from Position.x - 0.5 to Position.x + 0.5. It never
//    overlaps a platform's span, or it would spawn embedded in one.
//
//  * It keeps clear of every enemy spawn, so a fight never opens with a crate
//    already inside the enemy.
//
//  * Nothing sits within four units of x = 0, the same rule the enemies
//    follow - the player should not start the stage inside the scenery.
//
// One crate on the opening stage and two on every stage after it. They are
// meant to be a find rather than a supply line: with the 33/33/34 table in
// Crate::Break, nine crates across a run come out at roughly three health
// potions and three mana potions, and a stage can quite legitimately give
// nothing at all. HP carries between stages (see Game::s_CarriedHP), so that
// scarcity is the point - a potion matters because the next one is not
// around the corner.

// ---------------------------------------------------------------- STAGE 1
// Flat and open, one crate. Teaches the swing and the jump with nothing else
// going on, and every enemy is visible from the one before it.
static const EnemySpawn s_Stage1Enemies[] =
{
    { EnemyType::Walker, {  6.0f, 0.0f, 0.0f } },
    { EnemyType::Walker, { 19.0f, 0.0f, 0.0f } },
    { EnemyType::Walker, { 33.0f, 0.0f, 0.0f } },
    // The flier, last, once the swing and the jump have both been used. It
    // hovers above its target and out of reach of a swing taken standing up,
    // so it is the first thing that has to be jumped at rather than walked
    // into - which is why it arrives after the crate has taught the jump.
    { EnemyType::Flyer,  { 27.0f, 3.0f, 0.0f } },
};

static const BoxSpawn s_Stage1Boxes[] =
{
    { { 12.0f, 0.0f, 0.0f }, { 2.0f, 0.75f, 2.0f } }, // step,  x 10..14
    { { 26.0f, 0.0f, 0.0f }, { 2.0f, 1.00f, 2.0f } }, // block, x 24..28
};

static const CrateSpawn s_Stage1Crates[] =
{
    { {  16.5f, 0.0f, 0.0f } }, // past the first enemy, before the second
};

// ---------------------------------------------------------------- STAGE 2
// Introduces height. One enemy holds the middle platform, so the player has
// to climb to it instead of fighting everything on one line - and the crate
// at x 8..12 is the first thing that breaks line of sight.
static const EnemySpawn s_Stage2Enemies[] =
{
    { EnemyType::Walker,    { -4.0f, 0.0f, 0.0f } },
    { EnemyType::Flyer,     { 10.0f, 3.0f, 0.0f } },
    { EnemyType::Walker,    { 17.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    { 26.0f, 2.0f, 0.0f } }, // on the platform
    { EnemyType::Walker,    { 35.0f, 0.0f, 0.0f } },
    // The first turret. It cannot move and it is the only type that throws a
    // wave, so it turns the ground in front of it into something to cross
    // rather than something to walk over - and it is placed at the far end,
    // where there is room to see the shots coming.
    { EnemyType::Turret,    { 46.5f, 0.0f, 0.0f } },
    { EnemyType::Walker,    { 48.0f, 0.0f, 0.0f } },
};

static const BoxSpawn s_Stage2Boxes[] =
{
    { { 10.0f, 0.0f, 0.0f }, { 2.0f, 1.00f, 2.0f } }, // block,    x  8..12
    { { 26.0f, 0.0f, 0.0f }, { 4.0f, 1.00f, 2.0f } }, // platform, x 22..30
    { { 42.0f, 0.0f, 0.0f }, { 2.0f, 0.75f, 2.0f } }, // step,     x 40..44
};

static const CrateSpawn s_Stage2Crates[] =
{
    { {  14.5f, 0.0f, 0.0f } },
    { {  32.0f, 0.0f, 0.0f } },
};

// ---------------------------------------------------------------- STAGE 3
// A staircase up to the high ground in the middle, and a pocket behind the
// player's start so the map is not a single corridor running right.
static const EnemySpawn s_Stage3Enemies[] =
{
    // Guarding the pocket behind the start, which is otherwise a free corner.
    { EnemyType::Patroller, { -24.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    { -20.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    {   4.5f, 0.0f, 0.0f } },
    { EnemyType::Walker,    {  21.0f, 0.0f, 0.0f } },
    { EnemyType::Flyer,     {  24.0f, 3.0f, 0.0f } },
    { EnemyType::Walker,    {  31.0f, 2.5f, 0.0f } }, // holds the high ground
    { EnemyType::Walker,    {  42.0f, 0.0f, 0.0f } },
    // Covering the approach to the last crate, so the high ground has to be
    // taken under fire rather than at leisure.
    { EnemyType::Turret,    {  54.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    {  58.0f, 0.0f, 0.0f } },
};

static const BoxSpawn s_Stage3Boxes[] =
{
    { { -12.0f, 0.0f, 0.0f }, { 2.0f, 0.75f, 2.0f } }, // step,  x -14..-10
    { {   8.0f, 0.0f, 0.0f }, { 2.0f, 1.00f, 2.0f } }, // block, x   6..10
    { {  15.0f, 0.0f, 0.0f }, { 2.0f, 1.25f, 2.0f } }, // tall,  x  13..17  (the step above pairs with this)
    { {  31.0f, 0.0f, 0.0f }, { 5.0f, 1.25f, 2.0f } }, // long,  x  26..36
    { {  50.0f, 0.0f, 0.0f }, { 2.0f, 1.00f, 2.0f } }, // block, x  48..52
};

static const CrateSpawn s_Stage3Crates[] =
{
    { {  11.5f, 0.0f, 0.0f } }, // the gap between the block and the tall
    { {  45.0f, 0.0f, 0.0f } },
};

// ---------------------------------------------------------------- STAGE 4
// Longer, and broken into pockets - crates sit close enough together that the
// player rarely sees more than two enemies at once, so the fights arrive in
// waves rather than all at the start.
static const EnemySpawn s_Stage4Enemies[] =
{
    { EnemyType::Patroller, { -28.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    { -24.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    {  -8.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    {  18.0f, 0.0f, 0.0f } },
    { EnemyType::Turret,    {  20.0f, 0.0f, 0.0f } }, // in the gap before the platform
    { EnemyType::Walker,    {  26.0f, 2.0f, 0.0f } }, // on the first platform
    { EnemyType::Walker,    {  34.0f, 0.0f, 0.0f } },
    { EnemyType::Flyer,     {  46.0f, 3.0f, 0.0f } },
    { EnemyType::Walker,    {  54.0f, 2.0f, 0.0f } }, // on the second
    { EnemyType::Walker,    {  63.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    {  73.0f, 0.0f, 0.0f } },
};

static const BoxSpawn s_Stage4Boxes[] =
{
    { { -16.0f, 0.0f, 0.0f }, { 2.0f, 1.00f, 2.0f } }, // block,    x -18..-14
    { {   4.0f, 0.0f, 0.0f }, { 2.0f, 0.75f, 2.0f } }, // step,     x   2..6
    { {  13.0f, 0.0f, 0.0f }, { 2.0f, 1.25f, 2.0f } }, // tall,     x  11..15
    { {  26.0f, 0.0f, 0.0f }, { 4.0f, 1.00f, 2.0f } }, // platform, x  22..30
    { {  40.0f, 0.0f, 0.0f }, { 2.0f, 1.25f, 2.0f } }, // tall,     x  38..42
    { {  54.0f, 0.0f, 0.0f }, { 5.0f, 1.00f, 2.0f } }, // long,     x  49..59
    { {  68.0f, 0.0f, 0.0f }, { 2.0f, 0.75f, 2.0f } }, // step,     x  66..70
};

static const CrateSpawn s_Stage4Crates[] =
{
    { {   8.5f, 0.0f, 0.0f } },
    { {  45.0f, 0.0f, 0.0f } },
};

// ---------------------------------------------------------------- STAGE 5
// The longest, and the only one with two pieces of high ground held at once.
// The run ends here, so it is the one place the layout asks the player to use
// everything: climb for the platforms, break line of sight to pick fights one
// at a time, and hold ground on the long crate in the middle.
static const EnemySpawn s_Stage5Enemies[] =
{
    { EnemyType::Patroller, { -34.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    { -30.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    { -14.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    {   4.5f, 0.0f, 0.0f } },
    { EnemyType::Flyer,     {  20.0f, 3.0f, 0.0f } },
    { EnemyType::Walker,    {  21.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    {  30.0f, 2.5f, 0.0f } }, // the high middle
    { EnemyType::Turret,    {  38.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    {  40.0f, 0.0f, 0.0f } },
    { EnemyType::Flyer,     {  50.0f, 3.0f, 0.0f } },
    { EnemyType::Walker,    {  52.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    {  60.0f, 2.0f, 0.0f } }, // the second platform
    { EnemyType::Walker,    {  68.0f, 0.0f, 0.0f } },
    { EnemyType::Turret,    {  70.0f, 0.0f, 0.0f } },
    { EnemyType::Walker,    {  80.0f, 0.0f, 0.0f } },
};

static const BoxSpawn s_Stage5Boxes[] =
{
    { { -22.0f, 0.0f, 0.0f }, { 2.0f, 1.00f, 2.0f } }, // block,    x -24..-20
    { {  -6.0f, 0.0f, 0.0f }, { 2.0f, 0.75f, 2.0f } }, // step,     x  -8..-4
    { {   9.0f, 0.0f, 0.0f }, { 2.0f, 1.00f, 2.0f } }, // block,    x   7..11
    { {  16.0f, 0.0f, 0.0f }, { 2.0f, 1.25f, 2.0f } }, // tall,     x  14..18
    { {  30.0f, 0.0f, 0.0f }, { 5.0f, 1.25f, 2.0f } }, // long,     x  25..35
    { {  46.0f, 0.0f, 0.0f }, { 2.0f, 1.00f, 2.0f } }, // block,    x  44..48
    { {  60.0f, 0.0f, 0.0f }, { 4.0f, 1.00f, 2.0f } }, // platform, x  56..64
    { {  74.0f, 0.0f, 0.0f }, { 2.0f, 1.25f, 2.0f } }, // tall,     x  72..76
    { {  86.0f, 0.0f, 0.0f }, { 2.0f, 0.75f, 2.0f } }, // step,     x  84..88
};

static const CrateSpawn s_Stage5Crates[] =
{
    { {  12.5f, 0.0f, 0.0f } },
    { {  66.0f, 0.0f, 0.0f } },
};

// The run, in order. Add a stage by adding a line here - Game::Update reads
// GetStageCount(), so nothing else needs touching.
//
// Name, map left, map right, enemies, boxes, breakable crates. The maps get
// wider as the run
// goes on: 60 units, then 76, 92, 108 and 128.
static const StageData s_Stages[] =
{
    { "STAGE 1", -20.0f, 40.0f, s_Stage1Enemies, COUNT_OF(s_Stage1Enemies), s_Stage1Boxes, COUNT_OF(s_Stage1Boxes), s_Stage1Crates, COUNT_OF(s_Stage1Crates) },
    { "STAGE 2", -24.0f, 52.0f, s_Stage2Enemies, COUNT_OF(s_Stage2Enemies), s_Stage2Boxes, COUNT_OF(s_Stage2Boxes), s_Stage2Crates, COUNT_OF(s_Stage2Crates) },
    { "STAGE 3", -28.0f, 64.0f, s_Stage3Enemies, COUNT_OF(s_Stage3Enemies), s_Stage3Boxes, COUNT_OF(s_Stage3Boxes), s_Stage3Crates, COUNT_OF(s_Stage3Crates) },
    { "STAGE 4", -32.0f, 76.0f, s_Stage4Enemies, COUNT_OF(s_Stage4Enemies), s_Stage4Boxes, COUNT_OF(s_Stage4Boxes), s_Stage4Crates, COUNT_OF(s_Stage4Crates) },
    { "STAGE 5", -36.0f, 92.0f, s_Stage5Enemies, COUNT_OF(s_Stage5Enemies), s_Stage5Boxes, COUNT_OF(s_Stage5Boxes), s_Stage5Crates, COUNT_OF(s_Stage5Crates) },
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
