#pragma once

// One reward the player can be offered at the start of a map.
//
// A reward is plain data: which category it belongs to, which stat inside
// that category it touches, and by how much. RoguelikeSystem is the only
// place that turns that data into a gameplay change, so adding a new reward
// means adding one line to the reward pool - no new class, no virtual call,
// no allocation.

enum class RewardCategory
{
    Common, // affects the player itself (its Stats component / its movement)
    Weapon, // affects the weapon the player is holding
};

// Common stats - everything that lives on the player.
enum class CommonStat
{
    MaxHP,
    AttackPower,
    Defense,
    CriticalChance,
    MoveSpeed,
    JumpPower,
};

// Weapon stats - everything that lives on the equipped Weapon.
enum class WeaponStat
{
    Damage,
    AttackSpeed,
    Range,
    CriticalDamage,
};

struct RoguelikeReward
{
    RewardCategory Category = RewardCategory::Common;
    int Stat = 0;           // a CommonStat or a WeaponStat, depending on Category
    float Value = 0.0f;     // flat amount, or a ratio when Percent is true
    bool Percent = false;   // true: 0.15f means "+15%", false: 10.0f means "+10"
    const char* Name = "";  // short label, e.g. "+20 Max HP"
};

// CommonReward / WeaponReward exist only to fill in the category and stat
// for you. They add no data and no behaviour, so a reward built through
// them is still a plain RoguelikeReward that can be copied and stored by
// value:
//
//   CommonReward health(CommonStat::MaxHP, 20.0f, false, "+20 Max HP");
//   WeaponReward damage(WeaponStat::Damage, 0.15f, true, "+15% Damage");
struct CommonReward : public RoguelikeReward
{
    CommonReward(CommonStat Stat, float Value, bool Percent, const char* Name)
        : RoguelikeReward{ RewardCategory::Common, (int)Stat, Value, Percent, Name } {
    }
};

struct WeaponReward : public RoguelikeReward
{
    WeaponReward(WeaponStat Stat, float Value, bool Percent, const char* Name)
        : RoguelikeReward{ RewardCategory::Weapon, (int)Stat, Value, Percent, Name } {
    }
};