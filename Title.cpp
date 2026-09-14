#include "Title.h"
#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Polygon2D.h"
#include "input.h"
#include "Game.h"
#include "GameObject.h"
#include "audio.h"

void Title::Init()
{

	Polygon2D* background = Manager::AddGameObj<Polygon2D>();
	background->Init(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, L"asset\\bill_board\\tora.png");

	// A Scene is not a GameObject, so the track rides on the background image.
	// Both go away on the scene change, which is what stops it playing.
	Audio* bgm = background->AddGameComponent<Audio>(background);
	bgm->Load("asset\\Audio\\BGM\\TitleBGM.wav");
	bgm->Play(true);

}


void Title::Uninit()
{

}

void Title::Update()
{
	if (Input::GetKeyTrigger(VK_RETURN))
	{
		Game::ResetProgress();
		Manager::ChangeScene<Game>(3.0f);
	}
}

void Title::Draw()
{

}