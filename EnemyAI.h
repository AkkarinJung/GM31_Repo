#pragma once

#include <vector>
#include <functional>

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
    float AttackRange = 1.2f;
    float AttackCooldown = 1.5f;
    float AttackDuration = 0.4f; // how long the Attack state holds, i.e. the swing window

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
    bool  FaceTarget = true;     // false = face whichever way it is moving

    // Fully scripted movement. When set it replaces all steering above and
    // returns a desired move vector for this frame; states and attacks still
    // run as normal, so a scripted enemy can still be stunned or killed.
    std::function<Vector3(const EnemyAI&, float)> ScriptedMove;

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
    float m_BobTime = 0.0f;
    float m_PatrolDirection = 1.0f;

    bool m_AttackRequested = false;
    bool m_HomeCaptured = false;

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
    float HorizontalDistanceToTarget() const;
    Vector3 SeparationBias() const;

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

    // Events the owner reports back in.
    void OnDamaged();
    void OnDeath();

    GameObject* GetOwner() const { return m_GameObject; }
    GameObject* GetTarget() const { return m_Target; }
    const Vector3& GetHome() const { return m_Home; }
};