#include "main.h"
#include "renderer.h"
#include "RoguelikeSystem.h"
#include "RoguelikeUI.h"

#include "manager.h"
#include "input.h"
#include "Player.h"
#include "Stats.h"
#include "Weapon.h"

// The reward pool. This table is the only thing that has to change to add,
// remove or retune a reward - everything below just reads it.
// 4th arg: false = flat amount (5.0f -> "+5"), true = ratio (0.20f -> "+20%")
static const RoguelikeReward s_RewardPool[] =
{
    CommonReward(CommonStat::MaxHP,          20.0f, false, "+20 Max HP"),
    CommonReward(CommonStat::AttackPower,     3.0f, false, "+3 Attack"),
    CommonReward(CommonStat::Defense,         2.0f, false, "+2 Defense"),
    CommonReward(CommonStat::CriticalChance,  0.10f, true, "+10% Critical Chance"),
    CommonReward(CommonStat::MoveSpeed,       0.10f, true, "+10% Move Speed"),
    CommonReward(CommonStat::JumpPower,       0.15f, true, "+15% Jump Power"),

    WeaponReward(WeaponStat::Damage,          0.15f, true, "+15% Weapon Damage"),
    WeaponReward(WeaponStat::AttackSpeed,     0.10f, true, "+10% Attack Speed"),
    WeaponReward(WeaponStat::Range,           0.20f, true, "+20% Range"),
    WeaponReward(WeaponStat::CriticalDamage,  0.25f, true, "+25% Critical Damage"),
};

static const int s_RewardPoolSize = (int)(sizeof(s_RewardPool) / sizeof(s_RewardPool[0]));

std::vector<RoguelikeReward> RoguelikeSystem::s_Taken;

void RoguelikeSystem::Start(Player* Owner, int ChoiceCount)
{
    // Guard against a second pick. Scene::Init already runs once per map,
    // but this also covers a stray call from an update loop or a re-init.
    if (m_State != State::Inactive)
        return;

    m_Player = Owner;
    // The player was rebuilt with this stage, so put back everything the
    // run has already earned before offering anything new.
    for (int i = 0; i < (int)s_Taken.size(); i++)
        ApplyReward(s_Taken[i]);
    GenerateChoices(ChoiceCount);

    if (m_Choices.empty())
    {
        m_State = State::Done;
        return;
    }

    m_State = State::Selecting;

    m_UI = Manager::AddGameObj<RoguelikeUI>();
    m_UI->SetSystem(this);

    // Gameplay waits until a card is picked. Scene::Update keeps running,
    // which is what lets this system read input while paused.
    Manager::SetPause(true);
}

void RoguelikeSystem::Update()
{
    if (m_State != State::Selecting || m_UI == nullptr)
        return;

    // The UI owns the card layout, so it answers which card the cursor is
    // over - taking it stays this system's decision.
    int hovered = m_UI->GetCardIndexAt(Input::GetMouseX(), Input::GetMouseY());
    m_UI->SetHoveredIndex(hovered);

    if (hovered >= 0 && Input::GetKeyTrigger(VK_LBUTTON))
    {
        // Swallow the click. Gameplay resumes on this same frame and the
        // player also attacks on a left click - without this the pick would
        // start the map with a swing.
        Input::ConsumeKeyTrigger(VK_LBUTTON);
        SelectReward(hovered);
    }
}

void RoguelikeSystem::GenerateChoices(int Count)
{
    m_Choices.clear();

    if (Count > s_RewardPoolSize)
        Count = s_RewardPoolSize;

    // Draw without replacement so the same reward cannot appear twice in
    // one set of cards.
    std::vector<int> remaining;
    for (int i = 0; i < s_RewardPoolSize; i++)
        remaining.push_back(i);

    for (int i = 0; i < Count; i++)
    {
        int pick = rand() % (int)remaining.size();
        m_Choices.push_back(s_RewardPool[remaining[pick]]);
        remaining.erase(remaining.begin() + pick);
    }
}

void RoguelikeSystem::SelectReward(int Index)
{
    if (m_State != State::Selecting)
        return;

    if (Index < 0 || Index >= (int)m_Choices.size())
        return;

    ApplyReward(m_Choices[Index]);
    s_Taken.push_back(m_Choices[Index]); // kept so the next stage can re-apply it

    m_State = State::Done;
    m_Choices.clear();

    if (m_UI != nullptr)
    {
        m_UI->SetDestory();
        m_UI = nullptr;
    }

    Manager::SetPause(false);
}

void RoguelikeSystem::ApplyReward(const RoguelikeReward& Reward)
{
    switch (Reward.Category)
    {
    case RewardCategory::Common:
        ApplyCommon(Reward);
        break;

    case RewardCategory::Weapon:
        ApplyWeapon(Reward);
        break;
    }
}

void RoguelikeSystem::ApplyCommon(const RoguelikeReward& Reward)
{
    if (m_Player == nullptr)
        return;

    Stats* stats = m_Player->GetGameComponent<Stats>();

    switch ((CommonStat)Reward.Stat)
    {
    case CommonStat::MaxHP:
        // SetMaxHP also refills HP - harmless here, the pick happens
        // before the map starts and the player is at full HP anyway.
        if (stats != nullptr)
            stats->SetMaxHP(stats->GetMaxHP() + (int)Reward.Value);
        break;

    case CommonStat::AttackPower:
        if (stats != nullptr)
            stats->SetAttack(stats->GetAttack() + (int)Reward.Value);
        break;

    case CommonStat::Defense:
        if (stats != nullptr)
            stats->SetDefense(stats->GetDefense() + (int)Reward.Value);
        break;

    case CommonStat::CriticalChance:
        if (stats != nullptr)
            stats->SetCriticalChance(stats->GetCriticalChance() + Reward.Value);
        break;

    case CommonStat::MoveSpeed:
        m_Player->SetMoveSpeed(m_Player->GetMoveSpeed() * (1.0f + Reward.Value));
        break;

    case CommonStat::JumpPower:
        m_Player->SetJumpPower(m_Player->GetJumpPower() * (1.0f + Reward.Value));
        break;
    }
}

void RoguelikeSystem::ApplyWeapon(const RoguelikeReward& Reward)
{
    Weapon* weapon = m_Player != nullptr ? m_Player->GetWeapon() : nullptr;

    if (weapon == nullptr)
        return;

    switch ((WeaponStat)Reward.Stat)
    {
    case WeaponStat::Damage:
        weapon->SetDamage(weapon->GetDamage() * (1.0f + Reward.Value));
        break;

    case WeaponStat::AttackSpeed:
        // Faster attacks = shorter cooldown.
        weapon->SetCooldown(weapon->GetCooldown() / (1.0f + Reward.Value));
        break;

    case WeaponStat::Range:
        weapon->SetRange(weapon->GetRange() * (1.0f + Reward.Value));
        break;

    case WeaponStat::CriticalDamage:
        weapon->SetCriticalDamage(weapon->GetCriticalDamage() + Reward.Value);
        break;
    }
}