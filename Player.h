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
    const float m_AttackHitPoint = 0.35f;
    bool m_AttackHitDone = false;

    // How far into a swing the next one may start. Waiting for the full
    // animation (recovery included) is what made combos feel sluggish.
    const float m_ComboCancelPoint = 0.6f;
    const float m_AttackBufferTime = 0.35f;

    // A short freeze on impact, then the camera kick - the two cheapest
    // things that make a hit read as a hit.
    int m_HitStopFrames = 0;
    const int m_HitStopOnHit = 5;
    const float m_HitShake = 0.06f;

    // A small step into the swing, so an attack has weight behind it.
    const float m_AttackLunge = 3.0f;

    // Slash VFX. Purely visual: the hitbox is Sword::Use and the two are
    // triggered separately, so these can be tuned for looks alone.
    // Angles are a screen-space roll in radians - 0 is a flat horizontal
    // streak, positive rolls counter-clockwise.
    // Slash VFX. Purely visual: the hitbox is Sword::Use and the two are
    // triggered separately, so these are tuned for looks alone.
    //
    // Anchored to the player, NOT to the sword. The hand sits near the chest
    // and sweeps through a wide arc during the swing, so a sprite centred on
    // it landed somewhere different every time; measuring from the player
    // puts the arc in the same readable place on every swing.
    //
    // Not const, and not final: the debug keys in Update() move them live -
    // press F5 to print the numbers and paste them back here.
    // Offset from the player, all three axes. Forward runs along the way the
    // player faces, Height is straight up, Depth is world Z - the play plane
    // is z=0 and the camera looks down +Z, so positive Depth pushes the arc
    // away from the viewer and negative pulls it in front of the character.
    // Small, because the arc is meant to wrap AROUND the character (see the
    // reference art) rather than float out in front of him. Pushed too far
    // forward it stops reading as his swing at all, whatever the angle is.
    float m_SlashForward = 0.30f;
    float m_SlashHeight = 1.00f;
    float m_SlashDepth = -0.3f;    // slightly towards the camera, so the arc
                                   // passes in front of the body

    // Half-size of the crescent. The curve is baked into the texture, which
    // is square, so these stay close to each other - pulling them apart
    // squashes the arc rather than lengthening the swing. Raise both to
    // sweep a wider circle around the character.
    float m_SlashLength = 1.5f;
    float m_SlashThickness = 1.5f;

    // How far the arc turns as it travels, in radians. Small: the crescent
    // already reads as a swing, and spinning it far just looks like a wheel.
    float m_SlashSweep = 0.45f;

    // Live tuning offsets, added to every slash. The debug keys in Update()
    // move these - press F5 to print them and paste the numbers back.
    float m_SlashPitchTune = 0.0f;
    float m_SlashYawTune = 0.0f;
    float m_SlashRollTune = 0.0f;

    // One 3D pose per step of the combo, in radians.
    //
    // Roll is the one that has to be there: the arc is drawn bulging straight
    // DOWN, so it needs a quarter turn (added in SpawnSlash) to bulge the way
    // the player is facing. Pitch and yaw are what stop it reading as a flat
    // sticker - they tip the plane of the swing into the scene, so the arc
    // sweeps through depth rather than across the screen.
    const float m_SlashPitch[3] = { 0.15f, -0.18f,  0.22f };
    const float m_SlashYaw[3]   = { 0.28f,  0.38f, -0.32f };
    const float m_SlashRoll[3]  = { 0.00f, -0.45f,  0.45f };

    const float m_SlashLifetime = 0.18f; // short: a slash that lingers stops
                                         // reading as a fast one

    // The special/parry swing gets a bigger, slower, flatter one - it reads
    // as a heavier, more deliberate cut.
    const float m_SpecialSlashLength = 2.4f;
    const float m_SpecialSlashThickness = 2.4f;
    const float m_SpecialSlashSweep = 0.70f;
    const float m_SpecialSlashLifetime = 0.24f;
    const float m_SpecialSlashPitch = 0.10f;
    const float m_SpecialSlashYaw = 0.20f;

    void DebugTuneSlash();

    // Samples where the sword actually is, every frame, so SpawnSlash can
    // read off which way the blade is travelling.
    void TrackWeaponMotion();

    // Sword tracking. The direction the blade is MOVING is what the slash
    // should line up with - not the direction it is pointing, and certainly
    // not a fixed angle per combo step. Taking it from the motion means the
    // arc follows whatever the animation actually does, including the swings
    // whose fixed angles were wrong.
    Vector3 m_PrevWeaponPos{ 0.0f, 0.0f, 0.0f };
    Vector3 m_WeaponVelocity{ 0.0f, 0.0f, 0.0f };
    bool m_HasWeaponHistory = false;

    // How the arc's roll is decided. F10 cycles it live, so all three can be
    // compared on the same swing without a rebuild.
    //
    //   0  TABLE   the fixed per-combo m_SlashRoll values
    //   1  BLADE   which way the blade is pointing   <- default
    //   2  MOTION  which way the sword is travelling
    //
    // BLADE is the one that works. The crescent bulges along its own +X, and
    // the blade sticks out along the radius of the swing, so aiming the bulge
    // down the blade puts the arc exactly where the steel is. It is read
    // straight out of the sword's world matrix and is a unit vector, so
    // unlike everything below it never gets short and noisy.
    //
    // Two earlier attempts and why they failed, so they are not retried:
    //
    //   chest -> grip. GetMatrx() gives the GRIP, which sits in the hand and
    //   barely leaves the body: measured 0.166 units for the second combo
    //   step against 1.053 for the third, an 8x swing in what should be a
    //   steady reference. The blade does the sweeping, not the hand.
    //
    //   MOTION. A horizontal swing travels across the character, which in
    //   this game is world Z - straight into the screen. The play plane is
    //   XY, so almost nothing survives the projection and the angle comes
    //   out of rounding error. Kept for the swings where it does work.
    int m_SlashAngleMode = 1;

    // Below this the source vector is too short to take an angle from, and
    // the fixed table is used for that swing instead.
    const float m_SlashMotionMin = 0.01f;

    // The blade direction is a unit vector, so this only rejects the case
    // where the blade points almost straight into the screen and there is no
    // meaningful on-screen direction left.
    const float m_SlashBladeMin = 0.25f;

    // Logs the real numbers every time a slash spawns, which is the only
    // moment that matters - a key press samples whenever the key was hit,
    // which is usually not mid-swing. F11 turns it off once it has served
    // its purpose.
    bool m_SlashLogSpawn = true;

    //Right attack
    int m_RightAttackMPCost = 15;

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
    const float m_ParryTime = 0.5f;
    const int m_ParryMPReward = 10;   // part of the cost back for reading it right
    const float m_MPRegenPerSecond = 4.0f; // without this the parry runs dry and stops working
    float m_MPRegenCarry = 0.0f;
    const int m_ParryHitStop = 10;    // a heavier freeze than a normal hit
    const float m_ParryShake = 0.12f;

    void StartAttack();
    void StartRightAttack();

    // Spawns the swing's slash sprite. Visual only - never damage.
    void SpawnSlash();

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
};