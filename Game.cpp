#include "Game.h"

#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Polygon2D.h"
#include "Particle.h"
#include "Camera.h"
//#include "field.h"
#include "MeshField.h"
#include "Player.h"
#include "EnemyAI.h"
#include "Enemy.h"
#include "bullet.h"
#include "Explosion.h"
#include "Tree.h"
#include "Grass.h"
#include "Box.h"
#include "Crate.h"
#include "SkyDome.h"
#include "input.h"
#include "Score.h"

#include "GameObject.h"
#include "Result.h"
#include "Fade.h"
#include "Stats.h"

#include "HPBar.h"
#include "ControlsUI.h"
#include "StatsUI.h"
#include "Hedge.h"
#include "Prop.h"

#include "PotionBag.h"
#include "PotionSlotUI.h"

#include "StageUI.h"
#include "EnemyHPBar.h"
#include "Stage.h"

#include "RoguelikeSystem.h"
#include "SoundEffect.h"
#include "SlashEffect.h"

int Game::s_Stage = 0;
bool Game::s_RunComplete = false;
int Game::s_CarriedHP = Game::NoCarriedHP;

// Stage data only names an enemy type - this turns that into the AI preset
// the enemy is configured with, so the table stays free of engine types.
static EnemyAIConfig ConfigForType(EnemyType Type)
{
	switch (Type)
	{
	case EnemyType::Patroller: return EnemyAIConfig::Patroller();
	case EnemyType::Turret:    return EnemyAIConfig::Turret();
	case EnemyType::Flyer:     return EnemyAIConfig::Flyer();
	case EnemyType::Walker:
	default:                   return EnemyAIConfig::Walker();
	}
}

// How much tougher an enemy is on each stage after the first, as a fraction
// of its base added per stage. Stage 1 is always exactly the base, so these
// only ever make the run harder as it goes on - and they are linear rather
// than compounding, because at five stages a multiplier per stage runs away
// far faster than the player's one reward per stage can answer.
//
// At these values, across the five stages:
//     HP      30  40  51  61  72     (+35% of base per stage)
//     damage  20  24  28  32  36     (+20% of base per stage)
//
// HP climbs faster than damage on purpose. A longer fight is pressure the
// player can answer with skill; a bigger hit is just a bigger hit, and with
// HP carrying between stages now, damage compounds across the whole run.
static const float ENEMY_HP_PER_STAGE = 0.35f;
static const float ENEMY_DAMAGE_PER_STAGE = 0.20f;

static float StageScale(int Stage, float PerStage)
{
	if (Stage < 0)
		Stage = 0;

	return 1.0f + Stage * PerStage;
}

// Which mesh each type wears. The flyer is the one that is not a rabbit -
// a bee sells "hovers and comes at you through the air" in a way a ground
// animal lifted off the floor never did.
//
// Enemy::LoadModel picks its loader from the extension and fits an FBX to
// the collision body itself, so a new model only has to be dropped in and
// named here - no size to guess and no second code path.
static const char* ModelForType(EnemyType Type)
{
	switch (Type)
	{
	case EnemyType::Flyer: return "asset\\model\\Bee\\Bee.fbx";
	case EnemyType::Patroller:
	case EnemyType::Walker:
	case EnemyType::Turret:
	default:               return "asset\\model\\Rabbit\\rabbit_1.obj";
	}
}

// How hard each type is to shove around. A turret is a fixed emplacement
// and barely budges; a flier is light enough to knock aside.
static float MassForType(EnemyType Type)
{
	switch (Type)
	{
	case EnemyType::Turret: return 5.0f;
	case EnemyType::Flyer:  return 0.6f;
	case EnemyType::Patroller:
	case EnemyType::Walker:
	default:                return 1.0f;
	}
}

void Game::ResetProgress()
{
	s_Stage = 0;
	s_RunComplete = false;
	s_CarriedHP = NoCarriedHP; // a new run starts at full HP
	PotionBag::ResetRun();     // and with nothing in the slots
	RoguelikeSystem::ResetRun(); // a new run starts with no rewards carried over
	Score::ResetRun();           // and with nothing killed yet
}

// How far the player can walk either way. Set from the stage table at the
// top of Init(), before anything that reads them is built - each stage is a
// different size, and they run from 60 units across up to 128. The values
// here are only what a Game object starts life with.
float Game::MapLeft = -20.0f;
float Game::MapRight = 40.0f;

// The player is pinned to z = 0 and only ever moves in x and y, so two walls
// is the whole boundary. These run along z purely so they fill the screen -
// nothing can walk round them.
static const float HEDGE_SEGMENT_LENGTH = 4.0f;
static const int HEDGE_SEGMENTS = 3;

// A wall of hedges at each end of the map, turned a quarter so their long
// side runs across the camera. Collision::GatherSolids picks them up, so the
// invisible wall comes from the same objects and needs nothing else.
static void BuildMapEdge()
{
	const float quarterTurn = 1.5707963f;

	for (int side = 0; side < 2; side++)
	{
		float x = (side == 0) ? Game::MapLeft : Game::MapRight;

		for (int i = 0; i < HEDGE_SEGMENTS; i++)
		{
			// Centred on z = 0, which is the plane the player walks on.
			float z = (i - (HEDGE_SEGMENTS - 1) * 0.5f) * HEDGE_SEGMENT_LENGTH;

			Hedge* hedge = Manager::AddGameObj<Hedge>();
			hedge->SetPosition({ x, 0.0f, z });
			hedge->SetRotation({ 0.0f, quarterTurn, 0.0f });
		}
	}
}

// ---------------------------------------------------------------- scenery --
//
// COUNT_OF lives in Stage.cpp, which is a different translation unit.
#ifndef COUNT_OF
#define COUNT_OF(Array) ((int)(sizeof(Array) / sizeof(Array[0])))
#endif
//
// Background dressing. All of it sits behind the plane the player walks on -
// the camera looks down +z from z = -7, so anything at z = 0 would stand in
// the fight, and anything in front of that would hide it.

static const char* const s_TreeModels[] =
{
	"asset\\model\\Enviroment\\Tree\\tree.fbx",
	"asset\\model\\Enviroment\\Tree Large\\tree_large.fbx",
};

static const char* const s_MidModels[] =
{
	"asset\\model\\Enviroment\\Bush\\bush.fbx",
	"asset\\model\\Enviroment\\Bush Large\\bush_large.fbx",
	"asset\\model\\Enviroment\\Bench\\bench.fbx",
	"asset\\model\\Enviroment\\Street Lantern\\street_lantern.fbx",
	"asset\\model\\Enviroment\\Trashcan\\trashcan.fbx",
	"asset\\model\\Enviroment\\Hedge Straight\\hedge_straight.fbx",
};

static const char* const s_GroundModels[] =
{
	"asset\\model\\Enviroment\\Grass A\\grass_A.fbx",
	"asset\\model\\Enviroment\\Grass B\\grass_B.fbx",
	"asset\\model\\Enviroment\\Flower A\\flower_A.fbx",
	"asset\\model\\Enviroment\\Flower B\\flower_B.fbx",
	"asset\\model\\Enviroment\\Cobble Stones\\cobble_stones.fbx",
	"asset\\model\\Enviroment\\Cobble Stones Large\\cobble_stones_large.fbx",
	"asset\\model\\Enviroment\\Bird\\bird.fbx",
};

// One band of scenery: which models it draws from, how far back it sits, how
// far apart the pieces are, and the range of sizes. Three of these stacked
// back to front is what reads as depth rather than as a row of props.
struct SceneryBand
{
	const char* const* Models;
	int ModelCount;
	float NearZ, FarZ;
	float Spacing;            // average gap along x
	float MinScale, MaxScale;
};

static const SceneryBand s_SceneryBands[] =
{
	// Furthest back, so the biggest and the most spread out.
	{ s_TreeModels,   COUNT_OF(s_TreeModels),   10.0f, 15.0f, 7.0f, 0.90f, 1.30f },
	{ s_MidModels,    COUNT_OF(s_MidModels),     5.0f,  8.5f, 5.5f, 0.85f, 1.15f },
	// Just behind the player, small enough not to crowd the fight.
	{ s_GroundModels, COUNT_OF(s_GroundModels),  2.5f,  4.5f, 3.2f, 0.80f, 1.20f },
};

// How far past the map edge the scenery keeps going, so the background does
// not stop dead where the hedges do.
static const float SCENERY_OVERHANG = 6.0f;

// Fixed seed, so the background is the same every run instead of reshuffling
// itself whenever a stage reloads.
static unsigned int g_SceneryRandom = 0x2545f491u;

static float SceneryRandom01()
{
	g_SceneryRandom ^= g_SceneryRandom << 13;
	g_SceneryRandom ^= g_SceneryRandom >> 17;
	g_SceneryRandom ^= g_SceneryRandom << 5;
	return (g_SceneryRandom & 0xffffff) / (float)0x1000000;
}

static float SceneryRandomRange(float Min, float Max)
{
	return Min + (Max - Min) * SceneryRandom01();
}

// ------------------------------------------------------------- tree line --
//
// The scenery band above stops its mesh trees at 15 units. Past that a tree
// is never more than a shape, so a mesh would be 400 triangles and its own
// 4MB copy of the pack's atlas to say the same thing. Tree is the project's
// own camera-facing quad, already pointed at asset\bill_board\tree.png and
// already sharing that texture between every instance, so filling the horizon
// is only a matter of placing them.

// Tree's quad is 8 across and 10 tall in its own space, while tree.png is
// 350x504. Scaling both axes by the same number would come out fat, so x is
// pulled in to match the picture.
static const float TREE_QUAD_WIDTH = 8.0f;
static const float TREE_QUAD_HEIGHT = 10.0f;
static const float TREE_IMAGE_ASPECT = 350.0f / 504.0f;

// Far wider than the props nearer in. The view opens out with distance, so a
// band as narrow as the play area ends in bare sky at the screen edges.
static const float TREE_OVERHANG = 45.0f;

struct TreeBand
{
	float Density;            // trees per unit of x, NOT a fixed count
	float NearZ, FarZ;
	float MinHeight, MaxHeight;
};

// Density rather than a count, because the stages are no longer all the same
// width. A fixed 60 spread over the 128 unit stage 5 would be half as dense
// as the same 60 over stage 1, and the horizon would visibly thin out as the
// run went on. These are the old counts divided by the old span (130 units
// including the overhang), so stage 1 looks exactly as it always did.
static const TreeBand s_TreeBands[] =
{
	{ 0.46f, 18.0f, 34.0f, 4.5f, 7.0f },
	// Smaller and denser, so the horizon sits behind the row in front of it.
	{ 0.69f, 36.0f, 72.0f, 3.5f, 5.5f },
};

static void BuildTreeLine()
{
	const float startX = Game::MapLeft - TREE_OVERHANG;
	const float endX = Game::MapRight + TREE_OVERHANG;

	const int spanCount = (int)(endX - startX);

	for (int b = 0; b < COUNT_OF(s_TreeBands); b++)
	{
		const TreeBand& band = s_TreeBands[b];

		int count = (int)(spanCount * band.Density);

		for (int i = 0; i < count; i++)
		{
			float height = SceneryRandomRange(band.MinHeight, band.MaxHeight);

			Tree* tree = Manager::AddGameObj<Tree>();
			tree->SetPosition({ SceneryRandomRange(startX, endX), 0.0f,
				SceneryRandomRange(band.NearZ, band.FarZ) });

			tree->SetScale({ height * TREE_IMAGE_ASPECT / TREE_QUAD_WIDTH,
				height / TREE_QUAD_HEIGHT, 1.0f });

			// No EnableShadow: nothing this far back casts a shadow worth
			// drawing, and at these counts building one per tree only to
			// throw it away cost hundreds of file reads per stage load.
		}
	}
}

static void BuildScenery()
{
	g_SceneryRandom = 0x2545f491u; // reset, so stage 2 looks like stage 1 did

	const float startX = Game::MapLeft - SCENERY_OVERHANG;
	const float endX = Game::MapRight + SCENERY_OVERHANG;

	for (int b = 0; b < COUNT_OF(s_SceneryBands); b++)
	{
		const SceneryBand& band = s_SceneryBands[b];

		for (float x = startX; x < endX; x += band.Spacing)
		{
			// Jitter inside the slot rather than placing on the slot, so the
			// spacing does not read as a grid.
			float px = x + SceneryRandomRange(-band.Spacing * 0.35f, band.Spacing * 0.35f);
			float pz = SceneryRandomRange(band.NearZ, band.FarZ);

			// The map edge hedges run from z = -6 to 6, so anything nearer
			// than that at the same x would grow out of a wall.
			if (pz < 7.0f &&
				(fabsf(px - Game::MapLeft) < 2.5f || fabsf(px - Game::MapRight) < 2.5f))
				continue;

			const char* model = band.Models[(int)(SceneryRandom01() * band.ModelCount) % band.ModelCount];

			Prop* prop = Manager::AddGameObj<Prop>();
			prop->SetPosition({ px, 0.0f, pz });
			prop->SetRotation({ 0.0f, SceneryRandomRange(0.0f, 6.2831853f), 0.0f });
			prop->Load(model, SceneryRandomRange(band.MinScale, band.MaxScale));
		}
	}

	// One landmark, off to the side of where the fighting happens.
	Prop* fountain = Manager::AddGameObj<Prop>();
	fountain->SetPosition({ 4.0f, 0.0f, 9.5f });
	fountain->Load("asset\\model\\Enviroment\\Fountain\\fountain.fbx");
}

void Game::Init()
{
	// Everything that makes this stage different from the next comes out of
	// the stage table - see Stage.cpp. Read FIRST, because the map bounds
	// decide where the walls go, how far the tree line and the scenery run,
	// and where the camera stops - all of which are built below.
	const StageData& stage = GetStageData(s_Stage);

	MapLeft = stage.Left;
	MapRight = stage.Right;

	Manager::AddGameObj<Camera>();
	//Manager::AddGameObj<field>();
	Manager::AddGameObj<MeshField>();
	Manager::AddGameObj<SkyDome>();

	Player* player = Manager::AddGameObj<Player>();

	BuildMapEdge();
	BuildScenery();
	BuildTreeLine();

	for (int i = 0; i < stage.EnemyCount; i++)
	{
		const EnemySpawn& spawn = stage.Enemies[i];

		Enemy* enemy = Manager::AddGameObj<Enemy>();
		enemy->SetPosition(spawn.Position);
		enemy->LoadModel(ModelForType(spawn.Type));
		enemy->GetAI()->Configure(ConfigForType(spawn.Type));
		enemy->SetMass(MassForType(spawn.Type));

		// Stage difficulty. Computed here rather than read from Game inside
		// Enemy, the same way the AI preset and the mass are - which stage
		// the run is on is this scene's business, and how an enemy answers
		// it is the enemy's.
		enemy->ScaleForStage(StageScale(s_Stage, ENEMY_HP_PER_STAGE),
			StageScale(s_Stage, ENEMY_DAMAGE_PER_STAGE));
	}

	for (int i = 0; i < stage.BoxCount; i++)
	{
		const BoxSpawn& spawn = stage.Boxes[i];

		Box* box = Manager::AddGameObj<Box>();
		box->SetPosition(spawn.Position);
		box->SetScale(spawn.Scale);
	}

	// Breakable crates. Unlike the platforms above these carry no size - they
	// are all one unit, and what each one drops is rolled when it breaks, so
	// the layout only ever says where.
	for (int i = 0; i < stage.CrateCount; i++)
	{
		const CrateSpawn& spawn = stage.Crates[i];

		Crate* crate = Manager::AddGameObj<Crate>();
		crate->SetPosition(spawn.Position);
	}

	//Manager::AddGameObj<Grass>()->SetPosition({ 10.0f,0.0f,-5.0f });

	/*for (int i = 0; i < 300; i++)
	{
		Tree* tree = Manager::AddGameObj<Tree>();

		Vector3 position = {
			-100.0f + 200.0f * ((float)rand() / RAND_MAX),
			-1.0f,
			-100.0f + 200.0f * ((float)rand() / RAND_MAX)
		};
		tree->SetPosition(position);

		Vector3 scale = {
			2.0f,
			2.0f + 3.0f * ((float)rand() / RAND_MAX),
			1.0f
		};
		tree->SetScale(scale);

		Vector3 rot = {
			0.0f,
			0.0f,
			-0.1f + 0.2f * ((float)rand() / RAND_MAX)
		};
		tree->SetRotation(rot);
	}*/


	Manager::AddGameObj<Particle>()->SetPosition({ -2.0f,1.0f,-1.0f });

	// Top right, and the anchor is the RIGHT edge of the readout - Score
	// lays its digits out leftwards from here. It used to sit at 100,100 and
	// run 200 pixels to the right, straight through the potion slots.
	Manager::AddGameObj<Score>()->SetPosition({ SCREEN_WIDTH - 26.0f, 22.0f, 0.0f });
	Manager::AddGameObj<HPBar>()->Init(30.0f, 20.0f, 300.0f, 50.0f, player, BarStat::HP, L"asset\\texture\\UI_Bar\\bar_fill_red.png");
	Manager::AddGameObj<HPBar>()->Init(0.0f, 45.0f, 300.0f, 50.0f, player, BarStat::MP, L"asset\\texture\\UI_Bar\\bar_fill_blue.png");
	Manager::AddGameObj<PotionSlotUI>();
	Manager::AddGameObj<ControlsUI>();
	Manager::AddGameObj<StatsUI>();
	Manager::AddGameObj<StageUI>();
	Manager::AddGameObj<EnemyHPBar>();

	// Pull the slash frames in now. They are shared and loaded once, but
	// doing it lazily meant the first swing of the run stalled part way
	// through the animation while thirteen textures came off disk.
	SlashEffect::LoadShared();

	// The map is built - hand over to the reward pick before gameplay runs.
	// Scene::Init runs exactly once per map (Manager rebuilds the scene on
	// every change) and Start() ignores repeat calls, so the pick can never
	// happen twice. It pauses the game objects until a card is picked.
	m_Roguelike.Start(player, 3);

	// Damage carries between stages: the player is a new object at full HP,
	// so put back the HP the last stage ended on. This has to come after
	// Start - that is where the run's Max HP rewards are re-applied, and
	// restoring before them would clamp to a ceiling that is not the run's.
	if (s_CarriedHP != NoCarriedHP)
	{
		Stats* stats = player->GetGameComponent<Stats>();

		if (stats != nullptr)
			stats->SetHP(s_CarriedHP);
	}

	// Last, so it is on top of everything the map just built. It keeps
	// running through the reward pick's pause - see Fade and
	// GameObject::UpdatesWhilePaused - so the cards are dealt onto a screen
	// that has actually faded up.
	Fade::In(0.5f);
}


void Game::Uninit()
{

}

void Game::Update()
{
	m_Roguelike.Update();
	// Gameplay - including the "map cleared" check below - waits until the
	// player has picked a reward.
	if (m_Roguelike.IsSelecting())
		return;
	//if (Input::GetKeyTrigger(VK_RETURN))
	//{
	//	Manager::ChangeScene<Result>(5.0f);
	//}

	auto enemies = Manager::GetGameObjs<Enemy>();

	if (enemies.size() == 0 && !m_Cleared)
	{
		m_Cleared = true;

		SoundEffect::Play(SE::StageClear);

		if (s_Stage + 1 < GetStageCount())
		{
			// Next stage: the scene is rebuilt, so the map, the enemies and
			// the reward pick are all fresh. Rewards taken so far are
			// re-applied by RoguelikeSystem::Start.
			//
			// HP does not reset with the scene - whatever is left now is what
			// the next stage starts on, so it is read off the player before
			// the object goes away. Clearing a stage is not a heal.
			Player* player = Manager::GetGameObj<Player>();
			Stats* stats = player != nullptr ? player->GetGameComponent<Stats>() : nullptr;

			if (stats != nullptr)
				s_CarriedHP = stats->GetHP();

			s_Stage++;
			Manager::ChangeScene<Game>(3.0f);
			Fade::OutBefore(3.0f);
		}
		else
		{
			// Last stage cleared - the run is over.
			s_RunComplete = true;
			Manager::ChangeScene<Result>(3.0f);
			Fade::OutBefore(3.0f);
		}
	}
}

void Game::Draw()
{

}
