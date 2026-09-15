#pragma once

// One-shot sound effects.
//
// Deliberately NOT the Audio component. Audio is attached to a GameObject,
// so it dies with it - an enemy death sound would be destroyed by the same
// frame that kills the enemy - and it owns a single source voice that Play()
// stops and restarts, so two hits in the same moment cut each other off.
// This owns its own pool of voices per clip and outlives every scene, which
// is what a hit/death/parry sound needs.
//
//   Manager::Init()   -> SoundEffect::Init();
//   Manager::Uninit() -> SoundEffect::Uninit();
//   anywhere          -> SoundEffect::Play(SE::SwordHit);
//
// Every clip is listed in one table in SoundEffect.cpp - that table is the
// only thing to edit to change a file name or rebalance a volume.

// The game's vocabulary of sounds. Code asks for one of these; which file
// plays is the table's business.
enum class SE
{
    Jump,
    Land,

    // One per step of the 3-hit combo, so a combo sounds like a combo
    // instead of the same swing three times. StartAttack picks by
    // m_AttackCombo - keep these three adjacent and in order.
    PlayerAttack1,
    PlayerAttack2,
    PlayerAttack3,

    SwordHit,       // the swing connects
    SpecialAttack,  // right click - the parry stance
    Parry,          // it worked
    PlayerHurt,
    PlayerDeath,    // the run ends
    EnemyAttack,    // the wind-up, played with the red flash
    EnemyHurt,
    EnemyDeath,
    CardHover,
    CardSelect,
    StageClear,

    CrateBreak,     // a breakable crate bursts
    PotionPickup,   // what fell out of it goes into a slot
    PotionDrink,    // and is spent out of that slot

    Count           // keep last
};

class SoundEffect
{
public:
    // Loads every clip in the table. Safe to call after Audio::InitMaster
    // only - it borrows that XAudio2 device.
    static void Init();
    static void Uninit();

    // Fire and forget. A clip whose file is missing is silently skipped, so
    // a sound that has not been dropped into asset\Audio\SFX yet costs
    // nothing and crashes nothing.
    // Volume scales the clip's own table volume, for the odd call site that
    // wants a quieter or louder instance of the same sound.
    static void Play(SE Sound, float Volume = 1.0f);

    // 0.0f silences every effect; 1.0f is the table's own balance.
    static void SetMasterVolume(float Volume);
    static float GetMasterVolume();
};
