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
    Search,     // lost the target - go and look where it was last seen
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
    // stepped behind it mid-fight is worse than seeing too much. The WALL
    // check keeps applying either way; see LoseSightGrace.
    float VisionHalfAngle = 1.05f;   // ~60 degrees each side
    bool  RequireLineOfSight = true;
    float EyeHeight = 1.0f;          // looks from here, and at this height on the target

    // How often the line-of-sight segment test is actually run, in seconds.
    // The cone and the range checks are arithmetic and stay per-frame; the
    // wall test walks every solid in the map, so at one per enemy per frame a
    // crowded stage spent more time asking "can I see you" than drawing.
    // 0.1 is well inside a player's reaction time, and each enemy's phase is
    // rolled at Configure so a group does not all test on the same frame.
    float PerceptionInterval = 0.1f;

    // Reaction. Rolled per enemy inside this range at Configure, so a group
    // notices the player raggedly instead of as one animal.
    float ReactionMin = 0.20f;
    float ReactionMax = 0.40f;

    // Losing the target. A wall going up mid-fight does not erase what the
    // enemy already knows: it keeps its fix for this long, then goes to look
    // where the target was last actually seen rather than either snapping to
    // "never heard of you" or tracking through the crate forever.
    float LoseSightGrace = 1.2f;
    float SearchTime = 2.5f;          // how long it hunts before giving up
    float SearchSpeed = 2.0f;         // a hunting walk, not a charge
    float SearchArriveDistance = 0.6f;// close enough to the last known spot

    // Giving up. Walking into something for this long ends the chase - a
    // grounded enemy cannot climb, and shoving a crate forever is the single
    // most broken-looking thing an enemy can do.
    float BlockedGiveUpTime = 1.0f;
    float DisengageTime = 3.0f;      // ignores the player for this long afterwards

    // The other way a chase dies: not pushing into anything, just getting
    // nowhere. BlockedGiveUpTime only catches an enemy leaning on a crate,
    // because it compares the movement asked for against the movement got -
    // and an enemy that has QUIETLY STOPPED is asking for nothing, so it
    // accumulates nothing.
    //
    // That is the ledge case. A walker on a platform chases to the edge,
    // comes inside its own stopping distance in X of a player standing on the
    // ground below, stops because as far as it knows it has arrived, and can
    // then neither reach the player nor register as blocked. It stands on the
    // edge for the rest of the level. This clock is what sends it home: a
    // chase that neither closes the distance nor produces a swing for this
    // long is over.
    float ChaseGiveUpTime = 5.0f;

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

    // Hysteresis around StopDistance. The enemy stops at StopDistance and does
    // not start again until the target is StopDistance + StopBand away.
    // Without the band, a player standing exactly at the stopping distance
    // flipped the enemy between "walk" and "stand" every frame, which is the
    // vibrating-around-the-player bug in its purest form.
    float StopBand = 0.3f;

    // How fast the enemy may change its ground speed, in units per second per
    // second. Movement used to be a hard +/-1 direction at a fixed speed, so
    // every enemy started, stopped and reversed instantly. These two are what
    // make it lean into a charge and settle out of one; Deceleration is the
    // higher of the pair because a stop that lags reads as ice.
    float Acceleration = 14.0f;
    float Deceleration = 20.0f;

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
    // It also decides which attack queue the enemy waits in; see TokenSlot.
    bool  RangedAttack = false;
    float AttackRange = 1.2f;
    float AttackHeight = 1.5f;   // vertical band it will commit to a swing in -
                                 // keeps a grounded enemy from attacking a
                                 // player standing on a crate above it
    float AttackCooldown = 1.5f;
    float AttackDuration = 0.4f; // how long the Attack state holds, i.e. the swing window

    // Random spread on the cooldown, as a fraction either way (0.15 = +/-15%).
    // Two enemies that took damage on the same frame otherwise stay in lockstep
    // for the rest of the fight, and a pair swinging on the same beat reads as
    // one attack the player cannot answer rather than as two they can.
    float AttackCooldownVariance = 0.15f;

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

    // How hard a flier corrects its altitude: the vertical speed it asks for
    // per unit of height error, capped at ChaseSpeed. Setting it equal to
    // ChaseSpeed reproduces the hover this game has always had (the old code
    // clamped the error to +/-1 and multiplied it by the chase speed, which
    // is the same curve); raising it above that makes the hover stiffer and
    // lowering it makes the flier lag its own bob.
    float HoverGain = 2.0f;

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
//
// The contract with the owner is exactly one line:
//
//     velocity = GetMoveDirection() * GetMoveSpeed()
//
// GetMoveDirection is always unit length (or zero) and GetMoveSpeed carries
// the whole magnitude, so an enemy can never travel faster than the speed its
// config allows however many nudges went into the decision.
class EnemyAI : public Component
{
private:
    EnemyAIConfig m_Config;
    EnemyState m_State = EnemyState::Idle;

    class GameObject* m_Target = nullptr;
    class Stats* m_TargetStats = nullptr;  // cached with the target - a dead
                                           // target is not worth fighting
    float m_TargetScanTimer = 0.0f;        // throttles the search for a target

    class MeshField* m_MeshField = nullptr; // cached - a flier asked for it every frame

    Vector3 m_Home{ 0.0f, 0.0f, 0.0f };
    Vector3 m_MoveDirection{ 0.0f, 0.0f, 0.0f };
    float m_MoveSpeed = 0.0f;
    float m_Facing = 1.0f;

    // The smoothed ground velocity behind GetMoveDirection/GetMoveSpeed.
    // Signed, in units per second, so a turn-around eases through zero
    // instead of flipping the enemy on one frame.
    float m_VelocityX = 0.0f;

    float m_StateTime = 0.0f;
    float m_PauseTimer = 0.0f;
    float m_AttackCooldownTimer = 0.0f;
    float m_StunTimer = 0.0f;
    float m_StunImmunityTimer = 0.0f;
    float m_BobTime = 0.0f;
    float m_PatrolDirection = 1.0f;

    // Inside StopDistance and holding. Kept as a flag rather than recomputed
    // from the distance so the stop and the start can use different
    // thresholds - see EnemyAIConfig::StopBand.
    bool m_HoldingPosition = false;

    // Senses and reaction.
    float m_ReactionTime = 0.3f;   // rolled at Configure from the config range
    float m_ReactionTimer = 0.0f;
    bool  m_Aware = false;         // finished reacting, so it may act
    bool  m_Alerted = false;       // has been hit - sees through its own blind spot

    // Line of sight, sampled on an interval rather than every frame.
    float m_PerceptionTimer = 0.0f;
    bool  m_LineOfSight = false;
    float m_LostSightTimer = 0.0f; // how long the wall has been up

    // Where the target was when it was last actually seen, and how much
    // longer this enemy will keep looking for it there.
    Vector3 m_LastKnownPosition{ 0.0f, 0.0f, 0.0f };
    bool  m_HasLastKnown = false;
    float m_SearchTimer = 0.0f;

    // Giving up on an unreachable target.
    float m_BlockedTimer = 0.0f;
    float m_DisengageTimer = 0.0f;
    float m_LastX = 0.0f;

    // Chase progress. The closest this chase has managed so far, and how long
    // it has been since it last got closer or landed a swing. Negative best
    // distance means "not started" - the first update of a chase seeds it.
    float m_BestChaseDistance = -1.0f;
    float m_NoProgressTimer = 0.0f;

    bool m_AttackRequested = false;
    bool m_HomeCaptured = false;

    // Whoever currently has permission to swing, and the beat enforced between
    // one enemy giving it up and the next taking it.
    //
    // One queue per attack kind. With a single shared token a turret firing
    // from nine units away held the only permission in the level, so the
    // melee enemy standing on the player could not answer - two enemies with
    // nothing to do with each other were taking turns. Melee crowding is what
    // the token exists to pace; a ranged enemy is paced by its own range.
    static const int ATTACK_TOKEN_SLOTS = 2; // 0 = melee, 1 = ranged
    static EnemyAI* s_AttackToken[ATTACK_TOKEN_SLOTS];
    static float s_TokenCooldown[ATTACK_TOKEN_SLOTS];

    // Every live AI, used only for the separation check. A static list avoids
    // a manager class and keeps separation working for any GameObject that
    // carries this component, not just Enemy.
    static std::vector<EnemyAI*> s_Instances;

    void SetState(EnemyState State);
    void DecideState();
    void Steer();
    float VerticalVelocity();
    void UpdateFacing();
    void UpdateTarget();
    void UpdateBlocked();

    // True when this chase has stopped getting anywhere and should end.
    bool ChaseGaveUp(float Distance);

    // Everything the enemy knows about its target this frame, folded into one
    // answer: true while it has a fix on it.
    bool HasFixOnTarget(float Distance);
    bool TargetAlive() const;
    bool ConeContainsTarget() const;    // cheap, every frame
    bool HasLineOfSight();              // expensive, on PerceptionInterval
    float HorizontalDistanceToTarget() const;
    float AttackVerticalGap() const;    // same shape as Enemy::CanReachTarget
    bool FacingTarget() const;
    float SeparationBias() const;

    // Forget the target and, optionally, refuse to notice it again for a
    // while. One place, because the blocked-on-a-crate path and the
    // gave-up-searching path have to leave exactly the same state behind.
    void Disengage(float IgnoreTime);

    int  TokenSlot() const { return m_Config.RangedAttack ? 1 : 0; }
    bool TryTakeAttackToken();
    void ReleaseAttackToken();

public:
    using Component::Component;

    void Init() override;
    void Uninit() override;
    void Update() override;

    void Configure(const EnemyAIConfig& Config);
    void SetTarget(GameObject* Target);
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
    bool HasAttackToken() const { return s_AttackToken[TokenSlot()] == this; }

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

    // Development only, and free unless something asks for it. There is no
    // line renderer in this project, so this is a label rather than a gizmo:
    // print it with Font, or watch it in the debugger.
    const char* GetStateName() const;
};
