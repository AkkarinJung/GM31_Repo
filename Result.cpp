#include "Result.h"
#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Polygon2D.h"
#include "input.h"
#include "Title.h"
#include "Game.h"
#include "Score.h"
#include "Font.h"
#include "SoundEffect.h"
#include "audio.h"
#include "RoguelikeSystem.h"
#include "Stage.h"

#include "GameObject.h"

#include <stdio.h>
#include <math.h>

// Layout, in screen pixels.
//
// GameClear.png carries its own "Game Clear" plate, and it is NOT decoration
// that can be drawn over: measured off the image, it occupies x 330..952,
// y 242..389 once scaled to 1280x720. That plate IS the headline, so
// everything here sits below y=389 and nothing of ours is drawn on top of it.
//
// Two columns, because five rewards stacked under three summary rows would
// have run into the prompt at the bottom of the screen.
static const float PLATE_BOTTOM = 389.0f;

// Where the drawn headline sits, in the space the old painted plate used to
// occupy. Everything below still starts at PLATE_BOTTOM, so the summary
// layout is untouched.
static const float HEADLINE_Y = 250.0f;

static const float BODY_Y = PLATE_BOTTOM + 52.0f;
static const float SUMMARY_X = SCREEN_WIDTH * 0.30f;
static const float SUMMARY_GAP = 40.0f;

static const float REWARD_X = SCREEN_WIDTH * 0.70f;
static const float REWARD_GAP = 30.0f;

// The return button. Clickable as well as keyboard-driven - the reward pick
// is already played with the mouse, so ending a run on a keyboard-only prompt
// meant putting the mouse down for one keystroke.
static const float BUTTON_WIDTH = 330.0f;
static const float BUTTON_HEIGHT = 68.0f;
static const float BUTTON_Y = SCREEN_HEIGHT - 108.0f;

static float ButtonX()
{
    return SCREEN_WIDTH * 0.5f - BUTTON_WIDTH * 0.5f;
}

static bool CursorOverButton()
{
    float x = Input::GetMouseX();
    float y = Input::GetMouseY();

    return x >= ButtonX() && x <= ButtonX() + BUTTON_WIDTH &&
           y >= BUTTON_Y && y <= BUTTON_Y + BUTTON_HEIGHT;
}

// The rarity colours, matching the card frames the rewards were taken from
// and the list in StatsUI. A run that ended with a Legendary should be able
// to see that it did.
static const XMFLOAT4 COLOUR_BY_RARITY[(int)RewardRarity::Count] =
{
    { 0.78f, 0.76f, 0.74f, 1.0f }, // Common
    { 0.45f, 0.72f, 1.00f, 1.0f }, // Rare
    { 0.72f, 0.50f, 1.00f, 1.0f }, // Epic
    { 1.00f, 0.80f, 0.30f, 1.0f }, // Legendary
};

static const XMFLOAT4 COLOUR_LABEL = XMFLOAT4(0.66f, 0.70f, 0.76f, 1.0f);
static const XMFLOAT4 COLOUR_VALUE = XMFLOAT4(1.00f, 0.97f, 0.90f, 1.0f);
static const XMFLOAT4 COLOUR_SHADOW = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.65f);

// One summary line: a dim label ending at CentreX, a bright value starting
// just after it, so the numbers line up down the column.
static void DrawSummaryRow(const char* Label, const char* Value, float CentreX, float Y)
{
    const float size = 23.0f;

    float labelWidth = Font::Measure(Label, size);

    Font::Draw(Label, CentreX - 16.0f - labelWidth, Y, size, COLOUR_LABEL);
    Font::Draw(Value, CentreX + 16.0f, Y, size, COLOUR_VALUE);
}

void Result::Init()
{
    m_Time = 0.0f;

    // Read the run's numbers NOW. Manager has already torn down the Game
    // scene's objects, so the Score object is gone - only the static run
    // total it kept survives.
    m_Kills = Score::GetRunTotal();

    // s_Stage is left pointing at the final stage when the run completes
    // (Game::Update only advances it when there is a next one), so this is
    // how many were actually finished.
    m_Complete = Game::IsRunComplete();

    m_StagesCleared = m_Complete
        ? GetStageCount()
        : Game::GetStageIndex();

    // end_bg.png, and it carries NO text of its own - which is the point.
    // GameClear.png had "Game Clear" painted into it, so a run that ended
    // with the player dead still congratulated them. The headline is drawn
    // in the font now (see Draw) and says what actually happened.
    Manager::AddGameObj<Polygon2D>()->Init(
        0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT,
        L"asset\\texture\\end_bg.png");

    // The button frame, the same art the title menu uses.
    Manager::AddGameObj<Polygon2D>()->Init(
        ButtonX(), BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT,
        L"asset\\texture\\Button_Emty.png");
}

void Result::Uninit()
{
}

void Result::Update()
{
    m_Time += 1.0f / 60.0f;

    bool wasHovered = m_Hovered;
    m_Hovered = CursorOverButton();

    if (m_Hovered && !wasHovered)
        SoundEffect::Play(SE::CardHover);

    bool confirm = Input::GetKeyTrigger(VK_RETURN) || Input::GetKeyTrigger(VK_SPACE);

    if (m_Hovered && Input::GetKeyTrigger(VK_LBUTTON))
    {
        // Swallow it, the same way the reward pick does: left click is also
        // the attack button, and the next scene starts on this frame.
        Input::ConsumeKeyTrigger(VK_LBUTTON);
        confirm = true;
    }

    if (confirm)
    {
        SoundEffect::Play(SE::CardSelect);
        Manager::ChangeScene<Title>(3.0f);
    }
}

void Result::Draw()
{
    const float centreX = SCREEN_WIDTH * 0.5f;

    // The headline, which the background no longer supplies. It has to be
    // drawn rather than painted in, because the same screen ends a run that
    // was won and one that was lost and those are not the same sentence.
    const char* headline = m_Complete ? "RUN COMPLETE" : "YOU DIED";

    XMFLOAT4 headlineColour = m_Complete
        ? XMFLOAT4(1.00f, 0.86f, 0.42f, 1.0f)   // gold
        : XMFLOAT4(0.90f, 0.34f, 0.34f, 1.0f);  // red

    Font::DrawCentered(headline, centreX + 4.0f, HEADLINE_Y + 5.0f, 76.0f, COLOUR_SHADOW);
    Font::DrawCentered(headline, centreX, HEADLINE_Y, 76.0f, headlineColour);

    const char* sub = m_Complete
        ? "every stage cleared"
        : "the run ends here";

    Font::DrawCentered(sub, centreX + 2.0f, HEADLINE_Y + 88.0f + 2.0f, 21.0f, COLOUR_SHADOW);
    Font::DrawCentered(sub, centreX, HEADLINE_Y + 88.0f, 21.0f, COLOUR_LABEL);

    char value[64];

    // ---- left column: the run in numbers ----
    sprintf_s(value, "%d / %d", m_StagesCleared, GetStageCount());
    DrawSummaryRow("STAGES CLEARED", value, SUMMARY_X, BODY_Y);

    sprintf_s(value, "%d", m_Kills);
    DrawSummaryRow("ENEMIES DEFEATED", value, SUMMARY_X, BODY_Y + SUMMARY_GAP);

    const std::vector<RoguelikeReward>& taken = RoguelikeSystem::GetTaken();

    sprintf_s(value, "%d", (int)taken.size());
    DrawSummaryRow("REWARDS TAKEN", value, SUMMARY_X, BODY_Y + SUMMARY_GAP * 2.0f);

    // ---- right column: what the run was built out of ----
    if (!taken.empty())
    {
        Font::DrawCentered("REWARDS", REWARD_X, BODY_Y - 30.0f, 19.0f, COLOUR_LABEL);

        for (int i = 0; i < (int)taken.size(); i++)
        {
            int rarity = (int)taken[i].Rarity;
            if (rarity < 0 || rarity >= (int)RewardRarity::Count)
                rarity = 0;

            Font::DrawCentered(taken[i].Name, REWARD_X,
                BODY_Y + i * REWARD_GAP, 20.0f,
                COLOUR_BY_RARITY[rarity]);
        }
    }

    // The button label. It lights up under the cursor so the frame reads as
    // something to click rather than as decoration.
    float pulse = 0.78f + 0.22f * sinf(m_Time * 5.0f);

    XMFLOAT4 buttonColour = m_Hovered
        ? XMFLOAT4(1.0f, 0.86f * pulse + 0.14f, 0.45f * pulse, 1.0f)
        : XMFLOAT4(0.78f, 0.82f, 0.86f, 1.0f);

    float labelY = BUTTON_Y + BUTTON_HEIGHT * 0.5f - 14.0f;

    Font::DrawCentered("RETURN TO TITLE", centreX + 2.0f, labelY + 2.0f, 27.0f, COLOUR_SHADOW);
    Font::DrawCentered("RETURN TO TITLE", centreX, labelY, 27.0f, buttonColour);

    Font::DrawCentered("CLICK OR PRESS ENTER",
        centreX, SCREEN_HEIGHT - 30.0f, 16.0f,
        XMFLOAT4(0.62f, 0.66f, 0.70f, 0.50f));
}
