#pragma once

#include <vector>

#include "component.h"
#include "Vector3.h"

class EnemyAI;

enum class EnemyState
{
    Idle,
    Patrol,
    Chase,
    Attack,
    Stunned,
    Dead,
};

// Everything that makes one enemy type behave differently from another lives
// in here - there are no EnemyAI subclasses. The flags gate whole states off:
// an enemy with CanChase = false never enters Chase no matter how close the
// target gets, so "patrols but ignores the player" is a config, not a class.
struct EnemyAIConfig
{
    // Detection. 2.5D, so distance is measured along X with a vertical band -
    // a target directly overhead on a ledge is not "in front of" the enemy.
    bool  CanChase = false;
    float DetectRange = 0.0f;
    float LoseRange = 0.0f;      // kept larger than DetectRange so the enemy doesn't flicker in and out at the edge
    float DetectHeight = 2.5f;

    // What it can actually SEE, as opposed to what is in range. Detection used
    // to be a plain distance check, so an enemy spotted the player through a
    // crate and through the back of its own head - and a whole row of them
    // turned round in perfect unison the moment the player crossed a line.
    //
    // Half the cone, in radians, measured off the way the enemy is facing.
    // Once it is already fighting (or has been hit) the cone stops applying -
    // it knows where the player is, and losing track because the player
    // stepped behind it mid-fight is worse than seeing too much.
    float VisionHalfAngle = 1.05f;   // ~60 degrees each side
    bool  RequireLineOfSight = true;
    float EyeHeight = 1.0f;          // looks from here, and at this height on the target

    // Reaction. Rolled per enemy inside this range at spawn, so a group
    // notices the player raggedly instead of as one animal.
    float ReactionMin = 0.20f;
    float ReactionMax = 0.40f;

    // Giving up. Walking into something for this long ends the chase - a
    // grounded enemy cannot climb, and shoving a crate forever is the single
    // most broken-looking thing an enemy can do.
    float BlockedGiveUpTime = 1.0f;
    float DisengageTime = 3.0f;      // ignores the player for this long afterwards

    // Crowd control. Only the enemy holding the attack token may commit to a
    // swing; the rest keep their spacing and wait their turn. Without it every
    // enemy in reach swings the instant its own cooldown is up, which is what
    // makes a group read as unfair rather than as difficult.
    bool  NeedsAttackToken = true;

    // Patrol, measured each way from the spawn point.
    bool  CanPatrol = false;
    float PatrolRange = 0.0f;
    float PatrolSpeed = 1.5f;
    float PatrolPause = 0.0f;    // seconds spent idling at each end

    // Chase.
    float ChaseSpeed = 2.5f;
    float StopDistance = 1.0f;

    // Attack.
    bool  CanAttack = false;

    // How the attack is delivered. false = it swings where it stands and the
    // damage lands at the end of the telegraph; true = it throws a slash wave
    // that has to travel (see EnemyShot), and the damage arrives when the
    // wave does.
    //
    // This is the one line that makes an enemy ranged, so a preset changes
    // its whole role by flipping it - and the ranges below have to move with
    // it, because they mean reach for a swing and firing distance for a wave.
    bool  RangedAttack = false;
    float AttackRange = 1.2f;
    float AttackHeight = 1.5f;   // vertical band it will commit to a swing in -
                                 // keeps a grounded enemy from attacking a
                                 // player standing on a crate above it
    float AttackCooldown = 1.5f;
    float AttackDuration = 0.4f; // how long the Attack state holds, i.e. the swing window

    // How tall the thing it swings at is. The DECISION to attack and the swing
    // that follows have to measure reach the same way or the enemy burns a
    // whole attack state and cooldown on a swing that is then rejected - they
    // used to, because this lived on Enemy and the AI used a symmetric
    // fabs() instead. Enemy::CanReachTarget reads it back from here.
    float AttackTargetHeight = 1.8f;

    // Flight. When set the AI steers Y itself; otherwise the owner is free to
    // keep the enemy on the ground however it already does.
    bool  Flying = false;
    float HoverHeight = 2.0f;
    float BobAmplitude = 0.0f;
    float BobSpeed = 3.0f;

    // 2.5D spacing. A soft nudge apart so several enemies converging on the
    // same target fan out along X instead of stacking into one silhouette.
    // This is steering, not collision - the owner still resolves real overlap.
    // 0 disables it.
    float SeparationRadius = 0.0f;
    float SeparationStrength = 0.0f;

    float StunTime = 0.3f;

    // How long after a flinch this enemy refuses to flinch again. Damage
    // always lands; the REACTION is what this gates.
    //
    // Without it a 3 hit combo (a hit roughly every 0.3s) re-applied a 0.3s
    // stun on every hit, so an enemy in melee range was stun locked from the
    // first hit until the player chose to stop. It could never answer, which
    // is most of why a fight reads as hitting a training dummy. At 0.55 the
    // enemy is free to act between roughly every other hit, so a combo is a
    // trade rather than a lock.
    float StunImmunity = 0.55f;

    bool  FaceTarget = true;     // false = face whichever way it is moving

    // Presets - the intended way to add a new enemy "type".
    static EnemyAIConfig Patroller(); // walks a fixed beat, never reacts to the player
    static EnemyAIConfig Walker();    // patrols, then chases and melees once it spots the player
    static EnemyAIConfig Turret();    // never moves, attacks whatever comes close
    static EnemyAIConfig Flyer();     // hovers and drifts toward the player through the air
};

// Decision making only. This component decides what the enemy wants to do -
// which state it is in, where it wants to move, which way it should face and
// when it wants to swing. Acting on those decisions (moving, animating,
// dealing damage, rendering) stays with the owner GameObject.
class EnemyAI : public Component
{
private:
    EnemyAIConfig m_Config;
    EnemyState m_State = EnemyState::Idle;

    class GameObject* m_Target = nullptr;

    Vector3 m_Home{ 0.0f, 0.0f, 0.0f };
    Vector3 m_MoveDirection{ 0.0f, 0.0f, 0.0f };
    float m_MoveSpeed = 0.0f;
    float m_Facing = 1.0f;

    float m_StateTime = 0.0f;
    float m_PauseTimer = 0.0f;
    float m_AttackCooldownTimer = 0.0f;
    float m_StunTimer = 0.0f;
    float m_StunImmunityTimer = 0.0f;
    float m_BobTime = 0.0f;
    float m_PatrolDirection = 1.0f;

    // Senses and reaction.
    float m_ReactionTime = 0.3f;   // rolled at Init from the config range
    float m_ReactionTimer = 0.0f;
    bool  m_Aware = false;         // finished reacting, so it may act
    bool  m_Alerted = false;       // has been hit - sees through its own blind spot

    // Giving up on an unreachable target.
    float m_BlockedTimer = 0.0f;
    float m_DisengageTimer = 0.0f;
    float m_LastX = 0.0f;

    bool m_AttackRequested = false;
    bool m_HomeCaptured = false;

    // Whoever currently has permission to swing, and the beat enforced between
    // one enemy giving it up and the next taking it.
    static EnemyAI* s_AttackToken;
    static float s_TokenCooldown;

    // Every live AI, used only for the separation check. A static list avoids
    // a manager class and keeps separation working for any GameObject that
    // carries this component, not just Enemy.
    static std::vector<EnemyAI*> s_Instances;

    void SetState(EnemyState State);
    void DecideState();
    void Steer();
    void UpdateVertical();
    void UpdateFacing();

    bool TargetDetected(float Distance) const;
    bool CanSeeTarget() const;          // cone + line of sight
    float HorizontalDistanceToTarget() const;
    float AttackVerticalGap() const;    // same shape as Enemy::CanReachTarget
    bool FacingTarget() const;
    Vector3 SeparationBias() const;

    bool TryTakeAttackToken();
    void ReleaseAttackToken();

public:
    using Component::Component;

    void Init() override;
    void Uninit() override;
    void Update() override;

    void Configure(const EnemyAIConfig& Config) { m_Config = Config; }
    void SetTarget(GameObject* Target) { m_Target = Target; }
    void SetHome(const Vector3& Home) { m_Home = Home; m_HomeCaptured = true; }

    // Decisions for the owner to act on.
    EnemyState GetState() const { return m_State; }
    const Vector3& GetMoveDirection() const { return m_MoveDirection; }
    float GetMoveSpeed() const { return m_MoveSpeed; }
    float GetFacing() const { return m_Facing; } // -1 = facing -X, +1 = facing +X
    bool IsFlying() const { return m_Config.Flying; }
    bool ConsumeAttack(); // true once per swing, cleared when read

    // What the owner needs to time its own swing against.
    float GetAttackRange() const { return m_Config.AttackRange; }
    bool  IsRangedAttack() const { return m_Config.RangedAttack; }
    float GetAttackHeight() const { return m_Config.AttackHeight; }
    float GetAttackDuration() const { return m_Config.AttackDuration; }

    // What the owner needs to agree with when its swing lands.
    float GetAttackTargetHeight() const { return m_Config.AttackTargetHeight; }

    // True while this enemy is the one allowed to attack. The owner can use it
    // to tint or pose differently, so a crowd reads as "that one is coming".
    bool HasAttackToken() const { return s_AttackToken == this; }

    // Something hit it, or something it should react to happened nearby.
    // Cancels a disengage and lets it act without needing to see first.
    void Alert();

    // Events the owner reports back in.
    void OnDamaged();
    void Stun(float Time); // longer than a hit stun when the swing is parried
    void OnDeath();

    GameObject* GetOwner() const { return m_GameObject; }
    GameObject* GetTarget() const { return m_Target; }
    const Vector3& GetHome() const { return m_Home; }
};