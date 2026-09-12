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
#include "SkyDome.h"
#include "input.h"
#include "Score.h"

#include "GameObject.h"
#include "Result.h"

#include "HPBar.h"
#include "MPBar.h"
#include "ControlsUI.h"

#include "StageUI.h"
#include "Stage.h"

#include "RoguelikeSystem.h"

int Game::s_Stage = 0;

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

void Game::ResetProgress()
{
	s_Stage = 0;
	RoguelikeSystem::ResetRun(); // a new run starts with no rewards carried over
}

void Game::Init()
{
	

	Manager::AddGameObj<Camera>();
	//Manager::AddGameObj<field>();
	Manager::AddGameObj<MeshField>();
	Manager::AddGameObj<SkyDome>();

	Player* player = Manager::AddGameObj<Player>();

	// Everything that makes this stage different from the next comes out of
	// the stage table - see Stage.cpp.
	const StageData& stage = GetStageData(s_Stage);

	for (int i = 0; i < stage.EnemyCount; i++)
	{
		const EnemySpawn& spawn = stage.Enemies[i];

		Enemy* enemy = Manager::AddGameObj<Enemy>();
		enemy->SetPosition(spawn.Position);
		enemy->GetAI()->Configure(ConfigForType(spawn.Type));
	}

	for (int i = 0; i < stage.BoxCount; i++)
	{
		const BoxSpawn& spawn = stage.Boxes[i];

		Box* box = Manager::AddGameObj<Box>();
		box->SetPosition(spawn.Position);
		box->SetScale(spawn.Scale);
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

	Manager::AddGameObj<Score>()->SetPosition({ 100.0f,100.0f,0.0f });
	Manager::AddGameObj<HPBar>()->Init(30.0f, 20.0f, 300.0f, 50.0f, player, L"asset\\texture\\UI_Bar\\bar_fill_red.png");
	Manager::AddGameObj<MPBar>()->Init(0.0f, 45.0f, 300.0f, 50.0f, player, L"asset\\texture\\UI_Bar\\bar_fill_blue.png");
	Manager::AddGameObj<ControlsUI>();
	Manager::AddGameObj<StageUI>();

	// The map is built - hand over to the reward pick before gameplay runs.
	// Scene::Init runs exactly once per map (Manager rebuilds the scene on
	// every change) and Start() ignores repeat calls, so the pick can never
	// happen twice. It pauses the game objects until a card is picked.
	m_Roguelike.Start(player, 3);
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

		if (s_Stage + 1 < GetStageCount())
		{
			// Next stage: the scene is rebuilt, so the map, the enemies and
			// the reward pick are all fresh. Rewards taken so far are
			// re-applied by RoguelikeSystem::Start.
			s_Stage++;
			Manager::ChangeScene<Game>(3.0f);
		}
		else
		{
			// Last stage cleared - the run is over.
			Manager::ChangeScene<Result>(3.0f);
		}
	}
}

void Game::Draw()
{

}
