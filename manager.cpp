#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Camera.h"
#include "input.h"
#include "Title.h"
#include "Game.h"
#include "Result.h"
#include "audio.h"
#include "SoundEffect.h"
#include "SlashEffect.h"
#include "Font.h"

#include "GameObject.h"

std::list<GameObject*> Manager::m_GameObjects;
Scene* Manager::m_Scene = nullptr;
Scene* Manager::m_NextScene = nullptr;
float Manager::m_ChangeTime = 0.0f;
bool Manager::m_Pause = false;


void Manager::Init()
{
	Renderer::Init();
	Input::Init();
	Audio::InitMaster();
	SoundEffect::Init(); // after InitMaster - it borrows that XAudio2 device
	Font::Init(L"asset\\font\\kenvector_future.ttf", L"KenVector Future", 48);

	ChangeScene<Title>(0.0f);
}


void Manager::Uninit()
{
	if (m_Scene != nullptr)
		m_Scene->Uninit();

	for (GameObject* obj : m_GameObjects) {
		if (obj != nullptr) {
			obj->Uninit();
			delete obj;
			obj = nullptr;
		}
	}
	m_GameObjects.clear();

	Font::Uninit();
	SlashEffect::UninitShared(); // shared frames outlive every scene
	SoundEffect::Uninit(); // before UninitMaster - its voices feed the master
	Audio::UninitMaster();
	Renderer::Uninit();
	Input::Uninit();
}

void Manager::Update()
{
	float dt = 1.0f / 60.0f;
	Input::Update();

	if(m_Scene != nullptr)
	m_Scene->Update();

	if (!m_Pause)
	{
		for (GameObject* obj : m_GameObjects)
		{
			if (obj != nullptr) {
				obj->Update();
			}
		}
	}

	// Outside the pause. Pausing stops things THINKING; it was never meant to
	// keep dead objects alive, and leaving the reaper in there had a real
	// consequence: Game::Init builds 150 trees, each of which creates a shadow
	// in its own Init and is then told to drop it again (BuildTreeLine ->
	// Tree::HideShadow), and the very last thing Init does is start the reward
	// pick, which pauses. So 150 shadows sat marked-but-alive for the whole
	// card screen, at the origin because nothing had positioned them, each one
	// 40 units across - a huge black disc under the player that vanished the
	// moment a card was picked and the reaper was allowed to run.
	m_GameObjects.remove_if([](GameObject* object)
		{
			return object->Destory();
		});

	if (m_NextScene != nullptr)
	{
		m_ChangeTime -= dt;
		if (m_ChangeTime < 0.0f)
		{
			if (m_Scene != nullptr)
			{
				m_Scene->Uninit();
				delete m_Scene;
			}

			for (GameObject* obj : m_GameObjects)
			{
				if (obj != nullptr) {
					obj->Uninit();
					delete obj;
					obj = nullptr;
				}
			}

			m_GameObjects.clear();
			m_Pause = false; // a new scene never starts paused

			m_Scene = m_NextScene;
			m_Scene->Init();


			m_NextScene = nullptr;
		}
	}

}

void Manager::Draw()
{
	Renderer::Begin();
	Camera* camera = GetGameObj<Camera>();
	if (camera)
	{
		Vector3 forward = camera->GetFoward();
		Vector3 position = camera->GetPosition();

		for (GameObject* obj : m_GameObjects)
		{
			obj->CalcCameraZ(position, forward);
		}
		m_GameObjects.sort([](GameObject* a, GameObject* b)
			{
				return a->GetCameraZ() > b->GetCameraZ();
			});
	}

	for (int layer = 0; layer < 5; layer++)
	{
		for (GameObject* obj : m_GameObjects) {
			if (obj == nullptr || obj->IsDestroyed())
				continue; // marked this frame - do not show it one last time

			if (obj->GetLayer() == layer)
			{
				obj->Draw();
			}
		}
	}
	Renderer::End();
}
