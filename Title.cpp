#include "Title.h"
#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Polygon2D.h"
#include "input.h"
#include "Game.h"
#include "Loading.h"
#include "GameObject.h"
#include "audio.h"
#include "Font.h"
#include "SoundEffect.h"

#include <math.h>

// ---------------------------------------------------------------------------
// CHANGE THIS to whatever the game is called.
//
// It is the only place the name appears, and it is drawn in the font rather
// than baked into an image, so it costs nothing to rename.
//
// The ampersand is safe: Font bakes every glyph from 32 to 126 and '&' is
// 38, so it comes out of the same atlas as the letters.
// ---------------------------------------------------------------------------
static const char* GAME_TITLE = "DECK & LUCK";
static const char* GAME_SUBTITLE = "a roguelike game";

// Layout, in screen pixels.
static const float TITLE_Y = 140.0f;
static const float SUBTITLE_Y = 232.0f;

static const float BUTTON_WIDTH = 300.0f;
static const float BUTTON_HEIGHT = 74.0f;
static const float BUTTON_FIRST_Y = 380.0f;
static const float BUTTON_GAP = 96.0f;

// The menu. Two entries, and both of them do something - there is no
// HOW TO PLAY here on purpose: ControlsUI already shows the controls in game,
// and a menu entry that opens nothing is worse than one that is absent.
enum class TitleEntry
{
    Start,
    Exit,

    Count // keep last
};

static const char* ENTRY_LABEL[(int)TitleEntry::Count] =
{
    "START",
    "EXIT",
};

static const XMFLOAT4 COLOUR_TITLE = XMFLOAT4(1.00f, 0.97f, 0.90f, 1.0f);
static const XMFLOAT4 COLOUR_SUBTITLE = XMFLOAT4(0.72f, 0.80f, 0.76f, 1.0f);
static const XMFLOAT4 COLOUR_IDLE = XMFLOAT4(0.70f, 0.74f, 0.78f, 1.0f);
static const XMFLOAT4 COLOUR_SHADOW = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.65f);

// Where entry i's button sits.
static float EntryY(int Index)
{
    return BUTTON_FIRST_Y + Index * BUTTON_GAP;
}

static float EntryX()
{
    return SCREEN_WIDTH * 0.5f - BUTTON_WIDTH * 0.5f;
}

void Title::Init()
{
    m_Selected = 0;
    m_Time = 0.0f;

    // title_bg.png: a bright garden rather than the misty shrine bg01 used
    // to show. The shrine plate was atmospheric but it belonged to a
    // different game - this one is rabbits, bees, potions and a toon shader.
    //
    // It is composed around this menu: the scenery is pushed to the edges and
    // the horizon kept low, so the whole centre column the title, subtitle
    // and buttons sit on is plain sky. Nothing here needs a text plate.
    Polygon2D* background = Manager::AddGameObj<Polygon2D>();
    background->Init(0.0f, 0.0f, SCREEN_WIDTH, SCREEN_HEIGHT, L"asset\\texture\\title_bg.png");

    // One frame per entry. Polygon2D bakes its rectangle into its vertex
    // buffer and draws with an identity matrix, so these cannot move - which
    // is fine, the buttons are fixed and only the selection travels.
    for (int i = 0; i < (int)TitleEntry::Count; i++)
    {
        Manager::AddGameObj<Polygon2D>()->Init(
            EntryX(), EntryY(i), BUTTON_WIDTH, BUTTON_HEIGHT,
            L"asset\\texture\\Button_Emty.png");
    }

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
    m_Time += 1.0f / 60.0f;

    const int count = (int)TitleEntry::Count;
    int previous = m_Selected;

    // Keyboard.
    if (Input::GetKeyTrigger(VK_UP) || Input::GetKeyTrigger('W'))
        m_Selected = (m_Selected - 1 + count) % count;

    if (Input::GetKeyTrigger(VK_DOWN) || Input::GetKeyTrigger('S'))
        m_Selected = (m_Selected + 1) % count;

    // Mouse. Hovering moves the selection, which is what makes clicking feel
    // like it landed on the thing under the cursor.
    float mouseX = Input::GetMouseX();
    float mouseY = Input::GetMouseY();
    int hovered = -1;

    for (int i = 0; i < count; i++)
    {
        if (mouseX >= EntryX() && mouseX <= EntryX() + BUTTON_WIDTH &&
            mouseY >= EntryY(i) && mouseY <= EntryY(i) + BUTTON_HEIGHT)
        {
            hovered = i;
            break;
        }
    }

    if (hovered >= 0)
        m_Selected = hovered;

    if (m_Selected != previous)
        SoundEffect::Play(SE::CardHover); // the menu tick already exists

    bool confirm = Input::GetKeyTrigger(VK_RETURN) || Input::GetKeyTrigger(VK_SPACE);

    if (hovered >= 0 && Input::GetKeyTrigger(VK_LBUTTON))
    {
        // Swallow it. The game starts on this same frame and a left click is
        // also the attack button - without this the run opens with a swing.
        Input::ConsumeKeyTrigger(VK_LBUTTON);
        confirm = true;
    }

    if (!confirm)
        return;

    SoundEffect::Play(SE::CardSelect);

    switch ((TitleEntry)m_Selected)
    {
    case TitleEntry::Start:
        Game::ResetProgress();
        // Straight to the loading screen, with no fade of its own. The three
        // second wait that used to be here ran with the title still drawn and
        // START still lit, so the click looked like it had been missed - and
        // the real wait, Game::Init building the map, came AFTER it. Loading
        // puts something honest on screen and then asks for the map itself.
        Manager::ChangeScene<Loading>(0.0f);
        break;

    case TitleEntry::Exit:
        // The window's own close path, so everything shuts down the way it
        // does when the player hits the X - Manager::Uninit still runs.
        PostMessage(GetWindow(), WM_CLOSE, 0, 0);
        break;
    }
}

void Title::Draw()
{
    const float centreX = SCREEN_WIDTH * 0.5f;

    // Title, with a drop shadow. The shrine plate is busy and mid-toned, so
    // light text alone does not hold against it.
    Font::DrawCentered(GAME_TITLE, centreX + 3.0f, TITLE_Y + 4.0f, 82.0f, COLOUR_SHADOW);
    Font::DrawCentered(GAME_TITLE, centreX, TITLE_Y, 82.0f, COLOUR_TITLE);

    Font::DrawCentered(GAME_SUBTITLE, centreX + 2.0f, SUBTITLE_Y + 2.0f, 22.0f, COLOUR_SHADOW);
    Font::DrawCentered(GAME_SUBTITLE, centreX, SUBTITLE_Y, 22.0f, COLOUR_SUBTITLE);

    // The selected entry pulses and carries a marker either side. Polygon2D
    // cannot be moved after Init, so the arrow art cannot slide between the
    // buttons - this is the selection that CAN move.
    float pulse = 0.78f + 0.22f * sinf(m_Time * 5.0f);

    for (int i = 0; i < (int)TitleEntry::Count; i++)
    {
        bool selected = (i == m_Selected);

        float labelY = EntryY(i) + BUTTON_HEIGHT * 0.5f - 16.0f;
        float size = selected ? 34.0f : 30.0f;

        XMFLOAT4 colour = selected
            ? XMFLOAT4(1.0f, 0.86f * pulse + 0.14f, 0.45f * pulse, 1.0f)
            : COLOUR_IDLE;

        Font::DrawCentered(ENTRY_LABEL[i], centreX + 2.0f, labelY + 2.0f, size, COLOUR_SHADOW);
        Font::DrawCentered(ENTRY_LABEL[i], centreX, labelY, size, colour);

        if (!selected)
            continue;

        float half = Font::Measure(ENTRY_LABEL[i], size) * 0.5f;
        Font::Draw(">", centreX - half - 38.0f, labelY, size, colour);
        Font::Draw("<", centreX + half + 16.0f, labelY, size, colour);
    }

    Font::DrawCentered("ARROW KEYS OR MOUSE      ENTER TO CONFIRM",
        centreX, SCREEN_HEIGHT - 58.0f, 18.0f,
        XMFLOAT4(0.62f, 0.66f, 0.70f, 0.55f + 0.30f * sinf(m_Time * 2.2f)));
}
