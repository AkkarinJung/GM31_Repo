#pragma once

#include "Potion.h" // PotionType

// What the player is carrying.
//
// Two slots, filled in pickup order - either kind of potion can go in
// either slot. Walking over a potion stores it (see Potion::TryCollect);
// pressing its number key drinks it (see Player::Update). A potion found
// with both slots full is left on the ground, so nothing is ever wasted by
// walking past it.
//
// Deliberately not a GameObject and not a Component, for the same reason
// RoguelikeSystem is neither: it has no transform and nothing draws it.
// PotionSlotUI is a separate object that only reads what is held here.
//
// The state is static because it has to outlive the Player. Clearing a
// stage rebuilds the whole scene, and a potion carried into the boss fight
// should still be there - the same problem s_CarriedHP solves for HP.
// Game::ResetProgress empties it, so a new run starts with nothing.
class PotionBag
{
public:
    static const int SlotCount = 2;

    // Puts a potion in the first free slot. False when both are full, which
    // is the caller's cue to leave the pickup where it is.
    static bool TryStore(PotionType Type);

    // Drinks the potion in a slot and empties it. False - and nothing is
    // consumed - when the slot is empty, or when the bar it would fill is
    // already full: potions are scarce enough that a mistyped key should
    // not cost one.
    static bool Use(int Slot, class Player* Owner);

    static bool IsFilled(int Slot);
    static PotionType GetType(int Slot); // only meaningful when IsFilled

    // Call when a new run starts (Game::ResetProgress does), never between
    // stages - carrying a potion forward is the whole point.
    static void ResetRun();

private:
    static bool s_Filled[SlotCount];
    static PotionType s_Type[SlotCount];
};
