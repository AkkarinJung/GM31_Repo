#pragma once

#include "GameObject.h"
#include "animationModel.h"

class Player : public GameObject
{
private:
    Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f }; //‘¬“x

    ID3D11InputLayout* m_VertexLayout;
    ID3D11VertexShader* m_VertexShader;
    ID3D11PixelShader* m_PixelShader;

    bool m_Ground = true;

    // Collision body: half width, half height, half depth, standing on
    // m_Position. Everything solid is resolved against this one box.
    Vector3 m_BodyHalfSize{ 0.4f, 0.9f, 0.4f };
    // Movement tuning. Fields rather than literals in Update() so the
    // start-of-map rewards can scale them (see RoguelikeSystem).
    //
    // The jump has to clear the tallest crate in the stage table. A crate's
    // top is Scale.y * 2, so the tallest (1.25) stands at 2.5; power 25
    // reaches 2.98, which leaves room for a sloppy jump. Power 20 only
    // reached 1.877 and could not get onto any of them.
    float m_MoveSpeed = 50.0f;
    float m_JumpPower = 25.0f;

    GameObject* m_Shadow;

    class AnimationModel* m_AnimationModel;
    int m_AnimationFrame = 0;
    std::string m_AnimationName;

    int m_NextAnimationFrame = 0;
    std::string m_NextAnimationName;

    float m_Blend = 0.0f;

    bool m_Attacking = false;
    int m_AttackCombo = 0;
    int m_AttackAnimLength = 0;

    class Stats* m_Stats;

    class BoneAttachPoint* m_WeaponSocket;

    // How the sword sits in the hand, in bone space. One offset for every
    // state: the grip does not change because the arm swings, so the socket
    // is set once in Init() and left alone. Tune it live with the keys in
    // Update(), then press P and copy the numbers back here.
    Vector3 m_WeaponOffsetPos{ -4.6667f, 7.6667f, 0.0000f };
    Vector3 m_WeaponOffsetRot{ 0.0000f, 0.0000f, -1.0000f };
    class Weapon* m_Weapon; // base type on purpose - Use()/LoadModel() are virtual

    bool m_FreezeAnimation = false;

    float m_ComboResetTimer = 999.0f; // starts "expired" so the very first attack begins at Attack1
    const float m_ComboWindow = 0.6f; // seconds allowed between attacks to keep the combo going

    bool m_AttackQueued = false;
    float m_AttackBufferTimer = 0.0f; // a press is remembered this long, then dropped

    // Where in the swing the blade is actually dangerous, as a fraction of
    // the animation. Damage used to land the instant the button went down,
    // with the sword still behind the player - this is what made the hits
    // feel disconnected from the animation.
    //
    // It is a WINDOW, not an instant. A single frame of hitbox is 16ms: an
    // enemy that walks into the arc one frame late, or that is nudged out of
    // it by the crowd on exactly that frame, eats nothing and the swing looks
    // like it passed straight through. The weapon only lets a swing damage
    // each enemy once (Weapon::m_HitThisSwing), so widening this cannot
    // multi-hit - it only stops near misses of the clock.
    // Set per swing from SWING_SLICES in Player.cpp - they are fractions of
    // the SLICE being played, not of the whole clip, and every clip has its
    // own. Measured, not guessed: see DebugMeasureSwing.
    float m_AttackHitPoint = 0.35f;
    float m_AttackHitEnd = 0.55f;

    // The swing runs on its own clock instead of one animation key per game
    // frame, and the rate through it changes by phase.
    //
    // A constant rate is what makes a swing feel weightless: the windup, the
    // strike and the recovery all take the same time per key, so nothing
    // accelerates and there is no moment of impact. Easing into the windup
    // and then snapping through the contact frames is the single cheapest
    // thing that gives a swing weight, and it costs one float.
    //
    // The weight comes from the CONTRAST between these, not from any one of
    // them being slow. Strike at twice the windup rate is the whole trick.
    //
    // Dragging the windup out reads as anticipation but it also delays the
    // hit, and on a player character that is input lag. Faster windup plus a
    // much faster strike gets the contrast without costing response.
    //
    // WHY THE RATES ARE NO LONGER CONSTANTS. A fixed rate per key means the swing
    // takes as long as the artist made the clip. The attack clips here are
    // 137, 133, 181 and 91 keys at 60fps - 2.28s, 2.22s, 3.02s and 1.52s of
    // authored motion. At the old rates the hitbox opened 0.72s after the
    // button on Attack1 and 0.97s on Attack3. A Monster Hunter great sword,
    // the heaviest swing in the genre, lands at 0.60s. A light sword wants
    // 0.15-0.25s. The swing was not badly tuned, it was tuned in the wrong
    // unit: keys instead of seconds.
    //
    // It also made the combo inconsistent. Attack3 is 32% more keys than
    // Attack1, so the third hit of the combo was 35% slower than the first
    // for no reason other than clip length.
    //
    // So the timings below are declared in SECONDS and the per-phase rates
    // are DERIVED from them and the clip's own length, in SetupSwingClock().
    // Every swing now lands at the same moment regardless of which clip is
    // playing, and re-timing the combo is a question of what it should feel
    // like rather than of how many keys an artist happened to export.
    float m_AttackFrame = 0.0f;

    // What the swing should feel like, in seconds. These are the numbers to
    // tune - nothing else here.
    // Budgets set to what the clips actually contain. Attack1's blade is only
    // travelling for about six keys around contact and its follow-through is
    // ten - stretching those over 0.14s and 0.34s meant holding poses through
    // the fastest part of the swing, which with no interpolation between keys
    // reads as a stutter exactly where the swing should be sharpest.
    const float m_SwingWindupTime = 0.22f;  // press -> hitbox opens
    const float m_SwingActiveTime = 0.10f;  // how long it stays dangerous
    const float m_SwingRecoverTime = 0.18f; // contact -> swing over

    // The usable slice of the clip, as a fraction of it.
    //
    // A Mixamo clip settles out of neutral, swings, then returns to neutral.
    // The return is dead time the game does not want: the blend into Idle/Run
    // covers that transition already, so playing it just holds the player
    // still. Cutting the tail is what buys the recovery budget back.
    //
    // It matters MORE than it looks, because AnimationModel::Update() indexes
    // keys directly (f = Frame % numKeys) with no interpolation between them.
    // A rate of 4 does not play the clip smoothly at 4x, it shows every 4th
    // key and strobes. The shorter the slice, the lower the rate has to be,
    // and the smoother the swing actually looks.
    //
    // Set per swing from SWING_SLICES in Player.cpp. Measuring Attack1 showed
    // why these cannot be one global pair: its blade arrives at key 49 of 137
    // and the whole swing is spent by key 59. The other 78 keys are the
    // character walking its arm back to neutral, which the blend into
    // Idle/Run covers anyway. Every clip parks its contact somewhere else.
    float m_AttackClipStart = 0.00f;
    float m_AttackClipEnd = 0.80f;

    // Derived per swing from the above - do not set these by hand.
    float m_AttackClipFirst = 0.0f;
    float m_AttackClipLast = 0.0f;
    float m_AttackClipSpan = 1.0f;
    float m_AttackRateWindup = 1.10f;
    float m_AttackRateStrike = 2.20f;
    float m_AttackRateRecover = 1.30f;

    // Looks the clip up in SWING_SLICES, then works out the slice and the
    // three rates from it.
    void SetupSwingClock(const char* AnimationName);

    // How far into the swing the player gets control back. Movement used to
    // be locked for the WHOLE animation, so a three hit combo took the player
    // out of the fight for the length of three full clips with no way to step
    // out of anything. Recovery frames are meant to be a commitment, not a
    // cutscene.
    const float m_AttackMoveUnlock = 0.62f;
    const float m_AttackMoveScale = 0.55f;   // and at reduced speed until it ends

    // 0..1 through the current swing; 1 when not attacking.
    float AttackProgress() const;
    bool  MovementLocked() const;
    bool m_AttackHitDone = false; // the window has OPENED (vfx + sound spent)
    int m_AttackHitFrames = 0;    // frames of it left to run

    // How far into a swing the next one may start. Waiting for the full
    // animation (recovery included) is what made combos feel sluggish.
    const float m_ComboCancelPoint = 0.6f;
    const float m_AttackBufferTime = 0.35f;

    // A short freeze on impact, then the camera kick - the two cheapest
    // things that make a hit read as a hit.
    int m_HitStopFrames = 0;

    // ---- death ----------------------------------------------------------
    //
    // Nothing killed the player before this: Stats::IsDead() existed and was
    // only ever asked about enemies, so reaching 0 HP did nothing at all and
    // the run carried on. It ends the run now.
    bool  m_Dead = false;
    bool  m_ResultRequested = false;  // the scene change is asked for once
    float m_DeathTimer = 0.0f;
    float m_DeathFrame = 0.0f;

    // How long the body lies there before the result screen takes over. Long
    // enough for the animation to land and for the death sound to be heard
    // as the end of something rather than as one more hit.
    const float m_DeathHold = 2.6f;
    const float m_DeathAnimRate = 1.0f;  // keys per frame, like every other clip

    void BeginDeath();
    void UpdateDeath();
    // How hard each step of the combo lands.
    //
    // All three steps used to deal identical damage, freeze for identical
    // hitstop and kick the camera by an identical amount - which is why the
    // finisher never landed like one, and why the combo read as the same
    // swing three times rather than as something building.
    //
    // Indexed by m_AttackCombo (0..2). Kept as three tables side by side so a
    // fourth step cannot be added to one and forgotten in the others.
    const float m_ComboDamageScale[3] = { 1.00f, 1.15f, 1.50f };
    const int   m_ComboHitStop[3]     = {    4,     5,     9 };
    const float m_ComboShake[3]       = { 0.05f, 0.06f, 0.11f };

    // The counter is not a step in a chain - it is one heavy answer, bought
    // with MP and with the risk of reading the enemy's telegraph wrong - so
    // it lands off its own numbers rather than off the tables above.
    const float m_SpecialDamageScale = 1.60f;
    const int   m_SpecialHitStop = 10;
    const float m_SpecialShake = 0.13f;

    // Which of the above the swing in flight is using.
    float SwingDamageScale() const;
    int   SwingHitStop() const;
    float SwingShake() const;

    // How late a swing may still be turned round, as a fraction of the slice.
    // Facing is chosen when the button goes down and movement is locked for
    // the first 62% of the swing (m_AttackMoveUnlock), so an enemy that moved
    // behind the player between the press and the strike meant a swing into
    // empty air with nothing to be done about it. Up to this point the swing
    // can be redirected; after it the direction is committed, which is what
    // keeps the swing readable for the enemy and honest for the player.
    //
    // Set to the hit point so the window closes exactly as the blade comes
    // out: you may change your mind right up until it would be a lie.
    const float m_AttackTurnWindow = 0.35f;

    // A small step into the swing, so an attack has weight behind it.
    const float m_AttackLunge = 3.0f;

    //Right attack
    int m_RightAttackMPCost = 15;

    // The MP economy is no longer const. Reward cards move all four of these
    // (see CommonStat::MaxMP / MPRegen / SpecialCost / ParryReward /
    // ParryWindow), which they could not do while they were compile-time
    // constants.
public:
    int   GetSpecialMPCost() const { return m_RightAttackMPCost; }
    void  SetSpecialMPCost(int Cost) { m_RightAttackMPCost = Cost; }

    float GetMPRegenPerSecond() const { return m_MPRegenPerSecond; }
    void  SetMPRegenPerSecond(float Rate) { m_MPRegenPerSecond = Rate; }

    int   GetParryMPReward() const { return m_ParryMPReward; }
    void  SetParryMPReward(int Amount) { m_ParryMPReward = Amount; }

    float GetParryTime() const { return m_ParryTime; }
    void  SetParryTime(float Time) { m_ParryTime = Time; }

private:

    // The special attack parries. Its opening frames deflect an incoming
    // enemy attack instead of taking it, so the move is a read on the
    // enemy's telegraph rather than a damage button - and the MP cost is
    // what stops it from being mashed.
    bool m_SpecialAttacking = false;

    // Seconds, deliberately not a fraction of the animation: tying it to the
    // swing made the window depend on however long AttackRight happens to
    // be, and it vanished entirely if that animation failed to load.
    float m_ParryTimer = 0.0f;
    // Long enough that a fast reaction still covers the strike. The enemy
    // telegraph is ~0.63s, so a 0.35s window expired before the hit whenever
    // the player answered the flash quickly - which is what everyone does.
    float m_ParryTime = 0.5f;

    // Reading the telegraph is what pays for the next special, not waiting.
    //
    // The refund used to be 10 against a cost of 15, with 4 MP/s trickling in
    // underneath - which meant a full bar every 12.5s and a special every
    // 3.75s whether the player parried anything or not. The move was free, so
    // it was mashed. At 12 back on a 15 cost a successful parry runs at a net
    // 3 MP, while a whiffed one costs the full 15 and takes over eight
    // seconds to earn back. Same move, but now it is a read.
    int m_ParryMPReward = 12;
    float m_MPRegenPerSecond = 1.8f;
    float m_MPRegenCarry = 0.0f;
    const int m_ParryHitStop = 10;    // a heavier freeze than a normal hit
    const float m_ParryShake = 0.12f;

    void StartAttack();
    void StartRightAttack();

    // The AttackRight swing on its own. StartRightAttack pays MP for it; a
    // parry gets it free, because the parry already paid.
    void StartCounterAttack();

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    void SetAnimation(const char* AnimationName);
    // Read/written by the roguelike rewards.
    float GetMoveSpeed() const { return m_MoveSpeed; }
    void SetMoveSpeed(float MoveSpeed) { m_MoveSpeed = MoveSpeed; }

    float GetJumpPower() const { return m_JumpPower; }
    void SetJumpPower(float JumpPower) { m_JumpPower = JumpPower; }

    class Weapon* GetWeapon() const;

    // Called by an enemy whose swing is about to land. Returns true when the
    // special attack's parry frames were up, in which case the attack is
    // spent and the attacker should consider itself countered.
    bool TryParry(GameObject* Attacker);
    bool IsParrying() const;

    void DebugDumpSwing(const char* AnimationName, const char* BoneName);

    // Measures where the actual swing is inside a clip, by walking the clip
    // and watching how fast the weapon hand moves. The fastest key is the
    // contact frame; the stretch either side of it where the hand is still
    // moving is the part worth playing. Prints the m_AttackClipStart /
    // m_AttackClipEnd / m_AttackHitPoint those imply.
    void DebugMeasureSwing(const char* AnimationName, const char* BoneName);
};