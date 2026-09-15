#include "main.h"
#include "renderer.h"
#include "RoguelikeSystem.h"
#include "RoguelikeUI.h"

#include "manager.h"
#include "input.h"
#include "Player.h"
#include "Stats.h"
#include "Weapon.h"
#include "SoundEffect.h"

// The reward table. One line per EFFECT, not per card: the rarity roll
// decides the size, so a card that exists at four tiers is still one row.
//
// BaseValue is what the effect is worth at Common, in the units the apply
// switch further down expects. ShowAsPercent only changes how it PRINTS.
static const RewardDef s_RewardPool[] =
{
    // ---- the player ----
    { RewardCategory::Common, (int)CommonStat::MaxHP,          20.0f,  false, false, "Max HP" },
    { RewardCategory::Common, (int)CommonStat::AttackPower,     3.0f,  false, false, "Attack" },
    { RewardCategory::Common, (int)CommonStat::Defense,         2.0f,  false, false, "Defense" },
    { RewardCategory::Common, (int)CommonStat::CriticalChance,  0.08f, true,  false, "Critical Chance" },
    { RewardCategory::Common, (int)CommonStat::MoveSpeed,       0.08f, true,  false, "Move Speed" },
    { RewardCategory::Common, (int)CommonStat::JumpPower,       0.10f, true,  false, "Jump Power" },

    // ---- the MP economy ----
    // The special is the only thing that spends MP and the parry is the main
    // thing that returns it, so these are the cards that decide how often the
    // player can afford to answer a telegraph.
    { RewardCategory::Common, (int)CommonStat::MaxMP,          15.0f,  false, false, "Max MP" },
    { RewardCategory::Common, (int)CommonStat::MPRegen,         1.0f,  false, false, "MP per Second" },
    { RewardCategory::Common, (int)CommonStat::SpecialCost,     0.12f, true,  true,  "Special Cost" },
    { RewardCategory::Common, (int)CommonStat::ParryReward,     3.0f,  false, false, "MP per Parry" },
    { RewardCategory::Common, (int)CommonStat::ParryWindow,     0.10f, true,  false, "Parry Window" },

    // ---- the weapon ----
    { RewardCategory::Weapon, (int)WeaponStat::Damage,          0.15f, true,  false, "Weapon Damage" },
    { RewardCategory::Weapon, (int)WeaponStat::AttackSpeed,     0.10f, true,  false, "Attack Speed" },
    { RewardCategory::Weapon, (int)WeaponStat::Range,           0.15f, true,  false, "Range" },
    { RewardCategory::Weapon, (int)WeaponStat::CriticalDamage,  0.25f, true,  false, "Critical Damage" },
    { RewardCategory::Weapon, (int)WeaponStat::VerticalReach,   0.15f, true,  false, "Vertical Reach" },
};

static const int s_RewardPoolSize = (int)(sizeof(s_RewardPool) / sizeof(s_RewardPool[0]));

// What each tier multiplies BaseValue by.
//
// Deliberately gentle at the top. A Legendary is a good roll, not a different
// game: at 3.6x, the biggest move-speed card is +29%, which is noticeable
// without making the player impossible to control. Push the top entry much
// past 4 and the percentage cards start breaking things they were never
// balanced against.
static const float RARITY_SCALE[(int)RewardRarity::Count] =
{
    1.0f,  // Common
    1.8f,  // Rare
    2.6f,  // Epic
    3.6f,  // Legendary
};

// How often each tier comes up. Out of 100.
static const int RARITY_WEIGHT[(int)RewardRarity::Count] =
{
    60, // Common
    25, // Rare
    11, // Epic
    4,  // Legendary
};

const char* RarityName(RewardRarity Rarity)
{
    switch (Rarity)
    {
    case RewardRarity::Rare:      return "RARE";
    case RewardRarity::Epic:      return "EPIC";
    case RewardRarity::Legendary: return "LEGENDARY";
    case RewardRarity::Common:
    default:                      return "COMMON";
    }
}

std::vector<RoguelikeReward> RoguelikeSystem::s_Taken;

static RewardRarity RollRarity()
{
    int total = 0;
    for (int i = 0; i < (int)RewardRarity::Count; i++)
        total += RARITY_WEIGHT[i];

    int roll = rand() % total;

    for (int i = 0; i < (int)RewardRarity::Count; i++)
    {
        roll -= RARITY_WEIGHT[i];
        if (roll < 0)
            return (RewardRarity)i;
    }

    return RewardRarity::Common;
}

// Turns a definition plus a rarity into the card the player actually gets.
//
// The printed number and the applied number are rounded from the SAME value,
// so the card can never lie: if it says +36 Max HP the player gets exactly 36,
// not 36.4 rounded down somewhere else in the apply switch.
static RoguelikeReward BuildReward(const RewardDef& Def, RewardRarity Rarity)
{
    RoguelikeReward reward;
    reward.Category = Def.Category;
    reward.Rarity = Rarity;
    reward.Stat = Def.Stat;

    float scaled = Def.BaseValue * RARITY_SCALE[(int)Rarity];

    int shown;
    if (Def.ShowAsPercent)
    {
        shown = (int)(scaled * 100.0f + 0.5f);
        if (shown < 1) shown = 1;
        reward.Value = (float)shown / 100.0f;
    }
    else
    {
        shown = (int)(scaled + 0.5f);
        if (shown < 1) shown = 1;
        reward.Value = (float)shown;
    }

    sprintf_s(reward.Name, "%s%d%s %s",
        Def.ShowNegative ? "-" : "+",
        shown,
        Def.ShowAsPercent ? "%" : "",
        Def.Label);

    return reward;
}

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

    // Only when the cursor crosses onto a new card - playing it every frame
    // the mouse rests on one would be a buzz, not a tick.
    if (hovered >= 0 && hovered != m_HoveredIndex)
        SoundEffect::Play(SE::CardHover);

    m_HoveredIndex = hovered;
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

    // Draw the EFFECTS without replacement so the same stat cannot appear
    // twice in one set of cards. The rarity is rolled per card afterwards,
    // so two cards can share a tier - that part is meant to repeat.
    std::vector<int> remaining;
    for (int i = 0; i < s_RewardPoolSize; i++)
        remaining.push_back(i);

    for (int i = 0; i < Count; i++)
    {
        int pick = rand() % (int)remaining.size();
        m_Choices.push_back(BuildReward(s_RewardPool[remaining[pick]], RollRarity()));
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

    SoundEffect::Play(SE::CardSelect);

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
        // SetMaxHP grants the extra HP on top of what the player has rather
        // than refilling the bar, so re-applying this every stage cannot
        // turn a carried-over HP total back into a full one.
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

    // ---- the MP economy ----
    case CommonStat::MaxMP:
        // SetMaxMP still refills MP - unlike SetMaxHP, which no longer
        // refills now that HP carries between stages. Harmless: MP does not
        // carry, so the player starts every map with a full bar anyway.
        if (stats != nullptr)
            stats->SetMaxMP(stats->GetMaxMP() + (int)Reward.Value);
        break;

    case CommonStat::MPRegen:
        m_Player->SetMPRegenPerSecond(m_Player->GetMPRegenPerSecond() + Reward.Value);
        break;

    case CommonStat::SpecialCost:
    {
        // A reduction, and it must never reach zero or the special stops
        // being a decision. One MP is the floor.
        int cost = (int)(m_Player->GetSpecialMPCost() * (1.0f - Reward.Value) + 0.5f);
        if (cost < 1)
            cost = 1;
        m_Player->SetSpecialMPCost(cost);
        break;
    }

    case CommonStat::ParryReward:
        m_Player->SetParryMPReward(m_Player->GetParryMPReward() + (int)Reward.Value);
        break;

    case CommonStat::ParryWindow:
        m_Player->SetParryTime(m_Player->GetParryTime() * (1.0f + Reward.Value));
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
        weapon->SetDamageMultiplier(weapon->GetDamageMultiplier() * (1.0f + Reward.Value));
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

    case WeaponStat::VerticalReach:
        weapon->SetVerticalReach(weapon->GetVerticalReach() * (1.0f + Reward.Value));
        break;
    }
}