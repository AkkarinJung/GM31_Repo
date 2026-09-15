#include "main.h"
#include "renderer.h" // GameObject.h calls Renderer:: inline
#include "PotionBag.h"

#include "manager.h"
#include "Player.h"
#include "Stats.h"
#include "DamageNumber.h"
#include "SoundEffect.h"

// What a potion is worth, as a FRACTION of the bar it fills rather than a
// flat number. Max HP and Max MP are both reward cards - a Legendary Max HP
// roll alone is +72 - so a flat "restores 30" would start as a third of the
// bar and end the run as a rounding error.
static const float HEALTH_POTION_RATIO = 0.30f;
static const float MANA_POTION_RATIO = 0.40f;

static const XMFLOAT4 HEALTH_COLOUR = XMFLOAT4(0.35f, 1.0f, 0.40f, 1.0f);
static const XMFLOAT4 MANA_COLOUR = XMFLOAT4(0.40f, 0.65f, 1.0f, 1.0f);

bool PotionBag::s_Filled[PotionBag::SlotCount] = { false, false };
PotionType PotionBag::s_Type[PotionBag::SlotCount] = { PotionType::Health, PotionType::Health };

void PotionBag::ResetRun()
{
    for (int i = 0; i < SlotCount; i++)
        s_Filled[i] = false;
}

bool PotionBag::IsFilled(int Slot)
{
    if (Slot < 0 || Slot >= SlotCount)
        return false;

    return s_Filled[Slot];
}

PotionType PotionBag::GetType(int Slot)
{
    if (Slot < 0 || Slot >= SlotCount)
        return PotionType::Health;

    return s_Type[Slot];
}

bool PotionBag::TryStore(PotionType Type)
{
    for (int i = 0; i < SlotCount; i++)
    {
        if (s_Filled[i])
            continue;

        s_Filled[i] = true;
        s_Type[i] = Type;
        return true;
    }

    return false; // both full - the caller leaves the potion where it is
}

bool PotionBag::Use(int Slot, Player* Owner)
{
    if (!IsFilled(Slot) || Owner == nullptr)
        return false;

    Stats* stats = Owner->GetGameComponent<Stats>();

    if (stats == nullptr)
        return false;

    PotionType type = s_Type[Slot];

    // Refuse rather than waste it. With nine crates in a run and a third of
    // them empty, a potion drunk at full health is most of a stage's supply
    // gone to a mistyped key.
    if (type == PotionType::Health && stats->GetHP() >= stats->GetMaxHP())
        return false;

    if (type == PotionType::Mana && stats->GetMP() >= stats->GetMaxMP())
        return false;

    int amount;

    if (type == PotionType::Health)
    {
        amount = (int)(stats->GetMaxHP() * HEALTH_POTION_RATIO + 0.5f);
        if (amount < 1)
            amount = 1;

        stats->Heal(amount);
    }
    else
    {
        amount = (int)(stats->GetMaxMP() * MANA_POTION_RATIO + 0.5f);
        if (amount < 1)
            amount = 1;

        stats->RestoreMP(amount);
    }

    s_Filled[Slot] = false;

    SoundEffect::Play(SE::PotionDrink);

    // Above the player's head, so the number reads as something that
    // happened to the player. ShowSign, so a heal cannot be mistaken for the
    // damage numbers the enemies throw.
    Vector3 popupPos = Owner->GetPosition();
    popupPos.y += 2.2f;

    DamageNumber* popup = Manager::AddGameObj<DamageNumber>();
    popup->Init(popupPos, amount, true,
        type == PotionType::Health ? HEALTH_COLOUR : MANA_COLOUR);

    return true;
}
