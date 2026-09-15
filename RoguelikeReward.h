#pragma once

// One reward the player can be offered at the start of a map.
//
// A reward is plain data: which category it belongs to, which stat inside
// that category it touches, how rare it is, and by how much it moves the
// stat. RoguelikeSystem is the only place that turns that data into a
// gameplay change, so adding a new reward means adding one line to the
// reward table - no new class, no virtual call, no allocation.

enum class RewardCategory
{
    Common, // affects the player itself (its Stats component / its movement)
    Weapon, // affects the weapon the player is holding
};

// How good a roll of a card is. The card art carries four frame colours and
// four matching diamonds, which is exactly this ladder - grey, blue, purple,
// gold - so nothing new had to be drawn for it.
//
// Rarity does not change WHICH effect a card has, only how large it is: see
// RARITY_SCALE in RoguelikeSystem.cpp. That keeps every card useful at every
// tier and means one table drives the whole economy.
enum class RewardRarity
{
    Common,
    Rare,
    Epic,
    Legendary,

    Count // keep last
};

// "COMMON" / "RARE" / "EPIC" / "LEGENDARY", for the card face and the debug
// listing. Defined in RoguelikeSystem.cpp next to the tables it describes.
const char* RarityName(RewardRarity Rarity);

// Common stats - everything that lives on the player.
enum class CommonStat
{
    MaxHP,
    AttackPower,
    Defense,
    CriticalChance,
    MoveSpeed,
    JumpPower,

    // The MP economy. The special attack is the only thing that spends MP and
    // the parry is the main thing that gives it back, so these four are what
    // decide how often the player can afford to read an enemy.
    MaxMP,
    MPRegen,       // MP per second, passive
    SpecialCost,   // a REDUCTION - the value is subtracted as a ratio
    ParryReward,   // MP handed back for a successful parry
    ParryWindow,   // how long the deflect frames last
};

// Weapon stats - everything that lives on the equipped Weapon.
enum class WeaponStat
{
    Damage,
    AttackSpeed,
    Range,
    CriticalDamage,
    VerticalReach,
};

// The name is a buffer rather than a pointer because the text depends on the
// roll: a card reads "+20 Max HP" as a Common and "+72 Max HP" as a
// Legendary, and both are built at draw time. It is written once, by
// RoguelikeSystem::GenerateChoices, and only read after that.
struct RoguelikeReward
{
    RewardCategory Category = RewardCategory::Common;
    RewardRarity Rarity = RewardRarity::Common;
    int Stat = 0;           // a CommonStat or a WeaponStat, depending on Category
    float Value = 0.0f;     // already scaled by rarity, in the units the apply
                            // switch in RoguelikeSystem.cpp expects
    char Name[48] = "";     // e.g. "+20 Max HP" - generated, see BuildName
};

// What a card looks like before it is rolled: the effect, and what it is
// worth at Common. Everything above Common is this multiplied by
// RARITY_SCALE, so a card is one line here rather than four.
struct RewardDef
{
    RewardCategory Category;
    int Stat;
    float BaseValue;     // the Common-rarity amount, in apply units
    bool ShowAsPercent;  // print Value * 100 with a % sign
    bool ShowNegative;   // print a leading minus - the value itself stays positive
    const char* Label;   // "Max HP", "Damage", ...
};
