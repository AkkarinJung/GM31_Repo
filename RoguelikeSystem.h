#pragma once

#include <vector>
#include "RoguelikeReward.h"

// Start-of-map reward pick.
//
// Owned by the scene that starts a map (see Game) - it is deliberately not
// a GameObject and not a Component: it has no transform and nothing draws
// it. RoguelikeUI is a separate object that only reads the choices from
// here, so the reward logic never depends on UI code.
//
//   Game::Init()   -> m_Roguelike.Start(player);  // pauses gameplay, shows the cards
//   Game::Update() -> m_Roguelike.Update();       // reads the keys, applies, resumes
class RoguelikeSystem
{
private:
    enum class State
    {
        Inactive,  // the map has not started the pick yet
        Selecting, // cards are up, waiting for the player
        Done,      // a reward was applied - never runs again for this map
    };

    State m_State = State::Inactive;
    class Player* m_Player = nullptr;
    std::vector<RoguelikeReward> m_Choices;
    class RoguelikeUI* m_UI = nullptr;

    void ApplyCommon(const RoguelikeReward& Reward);
    void ApplyWeapon(const RoguelikeReward& Reward);

public:
    // Call once, when the map is finished building. Repeat calls are
    // ignored, so a re-init or a call from an update loop can never
    // re-roll a pick that already happened.
    void Start(class Player* Owner, int ChoiceCount = 3);

    // Drive from the scene's Update. Does nothing unless cards are up.
    void Update();

    void GenerateChoices(int Count);
    void SelectReward(int Index);
    void ApplyReward(const RoguelikeReward& Reward);

    bool IsSelecting() const { return m_State == State::Selecting; }
    const std::vector<RoguelikeReward>& GetChoices() const { return m_Choices; }
};