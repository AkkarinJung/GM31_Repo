#include "main.h"
#include "renderer.h"
#include <float.h>

#include "EnemyAI.h"

#include "GameObject.h"
#include "manager.h"
#include "Player.h"
#include "MeshField.h"

namespace
{
    // Same fixed step the rest of the game runs on.
    const float DELTA_TIME = 1.0f / 60.0f;
}

std::vector<EnemyAI*> EnemyAI::s_Instances;

EnemyAIConfig EnemyAIConfig::Patroller()
{
    EnemyAIConfig config;
    config.CanPatrol = true;
    config.PatrolRange = 4.0f;
    config.PatrolSpeed = 1.5f;
    config.PatrolPause = 0.5f;
    config.FaceTarget = false;

    // It still will not chase - that is what makes it a patroller - but it
    // defends itself when something walks into it. Without this the enemy
    // nearest the player's spawn just absorbed hits and never swung back,
    // which reads as a broken enemy rather than as a design choice.
    config.CanAttack = true;
    config.AttackRange = 1.3f;
    config.AttackHeight = 1.5f;
    config.AttackCooldown = 2.2f;  // slower than a Walker - it is not a fighter
    config.AttackDuration = 0.75f;

    // It has to notice the player to swing at one, and DetectRange defaults
    // to 0. Just past its own reach: enough to answer someone standing on it,
    // not enough to go looking.
    config.DetectRange = 2.0f;
    config.LoseRange = 3.0f;
    return config;
}

EnemyAIConfig EnemyAIConfig::Walker()
{
    EnemyAIConfig config = Patroller();
    config.CanChase = true;
    config.DetectRange = 6.0f;
    config.LoseRange = 9.0f;
    config.ChaseSpeed = 3.0f;
    config.StopDistance = 1.1f; // matches Enemy's separation - it closes to contact
    config.CanAttack = true;
    config.AttackRange = 1.4f; // just past touching - see m_AttackHeight in Enemy.h
    config.AttackHeight = 1.5f;
    // The enemy cannot move while attacking, so these two together decide how
    // much of its life it spends frozen. 0.75 of every 1.8 is ~40%, which
    // reads as "winds up, swings, recovers, comes at you again". At 0.9 of
    // every 1.2 it was frozen 75% of the time and looked broken.
    config.AttackCooldown = 1.8f;
    config.AttackDuration = 0.75f; // telegraph is 70% of this - see Enemy.h
    config.FaceTarget = true;
    config.SeparationRadius = 1.4f;
    config.SeparationStrength = 1.0f;
    return config;
}

EnemyAIConfig EnemyAIConfig::Turret()
{
    EnemyAIConfig config;
    config.CanAttack = true;
    config.AttackRange = 1.8f;    // it cannot move, so it needs a little more reach
    config.AttackHeight = 1.8f;
    config.AttackCooldown = 1.8f;
    config.AttackDuration = 0.75f;
    config.DetectRange = 2.0f;
    config.LoseRange = 3.0f;
    return config;
}

EnemyAIConfig EnemyAIConfig::Flyer()
{
    EnemyAIConfig config;
    config.CanChase = true;
    config.DetectRange = 7.0f;
    config.LoseRange = 10.0f;
    config.ChaseSpeed = 2.2f;
    config.StopDistance = 1.2f;
    config.CanAttack = true;
    config.AttackRange = 1.4f;
    config.AttackHeight = 3.0f;   // it hovers 2.5 above its target
    config.AttackCooldown = 1.8f;
    config.AttackDuration = 0.7f;
    config.Flying = true;
    config.HoverHeight = 2.5f;
    config.BobAmplitude = 0.3f;
    config.BobSpeed = 2.5f;
    config.SeparationRadius = 1.6f;
    config.SeparationStrength = 1.2f;
    return config;
}

void EnemyAI::Init()
{
    s_Instances.push_back(this);
}

void EnemyAI::Uninit()
{
    for (size_t i = 0; i < s_Instances.size(); i++)
    {
        if (s_Instances[i] == this)
        {
            s_Instances.erase(s_Instances.begin() + i);
            break;
        }
    }
}

void EnemyAI::Update()
{
    if (!m_HomeCaptured)
    {
        // Spawn position is assigned after Init() runs
        // (Manager::AddGameObj<T>()->SetPosition(...)), so the patrol anchor
        // can only be read once the object has actually been placed.
        m_Home = m_GameObject->GetPosition();
        m_HomeCaptured = true;
    }

    if (m_State == EnemyState::Dead)
        return;

    if (m_Target == nullptr)
        m_Target = Manager::GetGameObj<Player>();

    m_StateTime += DELTA_TIME;

    if (m_AttackCooldownTimer > 0.0f)
        m_AttackCooldownTimer -= DELTA_TIME;

    DecideState();
    Steer();
}

void EnemyAI::SetState(EnemyState State)
{
    if (m_State == State)
        return;

    m_State = State;
    m_StateTime = 0.0f;
}

void EnemyAI::DecideState()
{
    if (m_State == EnemyState::Stunned)
    {
        m_StunTimer -= DELTA_TIME;
        if (m_StunTimer > 0.0f)
            return;

        SetState(EnemyState::Idle);
    }

    if (m_State == EnemyState::Attack)
    {
        if (m_StateTime < m_Config.AttackDuration)
            return; // let the swing finish before reconsidering

        SetState(EnemyState::Idle);
    }

    float distance = HorizontalDistanceToTarget();
    bool detected = TargetDetected(distance);

    // Vertical gap as well as horizontal. Deciding on horizontal distance
    // alone meant an enemy committed to a swing it could not reach, spending
    // its whole attack state and cooldown standing frozen with nothing to
    // show for it.
    float verticalGap = (m_Target != nullptr)
        ? fabsf(m_Target->GetPosition().y - m_GameObject->GetPosition().y)
        : 0.0f;

    if (m_Config.CanAttack && detected &&
        distance <= m_Config.AttackRange &&
        verticalGap <= m_Config.AttackHeight &&
        m_AttackCooldownTimer <= 0.0f)
    {
        SetState(EnemyState::Attack);
        m_AttackRequested = true;
        m_AttackCooldownTimer = m_Config.AttackCooldown;
        return;
    }

    if (m_Config.CanChase && detected)
    {
        SetState(EnemyState::Chase);
        return;
    }

    if (m_Config.CanPatrol)
    {
        if (m_State == EnemyState::Idle)
        {
            m_PauseTimer -= DELTA_TIME;
            if (m_PauseTimer <= 0.0f)
                SetState(EnemyState::Patrol);
        }
        else if (m_State != EnemyState::Patrol)
        {
            SetState(EnemyState::Patrol);
        }
        return;
    }

    SetState(EnemyState::Idle);
}

void EnemyAI::Steer()
{
    m_MoveDirection = { 0.0f, 0.0f, 0.0f };
    m_MoveSpeed = 0.0f;

    bool canMove = (m_State != EnemyState::Stunned && m_State != EnemyState::Attack);

    if (canMove)
    {
        switch (m_State)
        {
        case EnemyState::Patrol:
        {
            float offset = m_GameObject->GetPosition().x - m_Home.x;

            bool atEnd =
                (m_PatrolDirection > 0.0f && offset >= m_Config.PatrolRange) ||
                (m_PatrolDirection < 0.0f && offset <= -m_Config.PatrolRange);

            if (atEnd)
            {
                m_PatrolDirection = -m_PatrolDirection;

                if (m_Config.PatrolPause > 0.0f)
                {
                    m_PauseTimer = m_Config.PatrolPause;
                    SetState(EnemyState::Idle);
                    break; // stand still this frame
                }
            }

            m_MoveDirection.x = m_PatrolDirection;
            m_MoveSpeed = m_Config.PatrolSpeed;
            break;
        }

        case EnemyState::Chase:
        {
            if (m_Target == nullptr)
                break;

            float dx = m_Target->GetPosition().x - m_GameObject->GetPosition().x;

            if (fabsf(dx) > m_Config.StopDistance)
            {
                m_MoveDirection.x = (dx > 0.0f) ? 1.0f : -1.0f;
                m_MoveSpeed = m_Config.ChaseSpeed;
            }
            break;
        }

        default:
            break;
        }
    }

    if (canMove)
    {
        Vector3 separation = SeparationBias();
        m_MoveDirection += separation;

        // separation still has to move the enemy after it stops closing in,
        // otherwise several enemies stack on the same spot
        if (m_MoveSpeed <= 0.0f && fabsf(separation.x) > 0.001f)
            m_MoveSpeed = m_Config.ChaseSpeed;
    }

    UpdateVertical();
    UpdateFacing();
}

void EnemyAI::UpdateVertical()
{
    if (!m_Config.Flying)
        return;

    m_BobTime += DELTA_TIME;

    float baseY;
    if (m_State == EnemyState::Chase && m_Target != nullptr)
    {
        baseY = m_Target->GetPosition().y + m_Config.HoverHeight;
    }
    else
    {
        MeshField* meshField = Manager::GetGameObj<MeshField>();
        float ground = (meshField != nullptr)
            ? meshField->GetHeight(m_GameObject->GetPosition())
            : m_Home.y;
        baseY = ground + m_Config.HoverHeight;
    }

    float desiredY = baseY + sinf(m_BobTime * m_Config.BobSpeed) * m_Config.BobAmplitude;
    float dy = desiredY - m_GameObject->GetPosition().y;

    if (dy > 1.0f) dy = 1.0f;
    if (dy < -1.0f) dy = -1.0f;
    m_MoveDirection.y = dy;

    // A hovering enemy still needs a speed to hold its altitude with, even
    // when it has decided not to travel anywhere this frame.
    if (m_MoveSpeed <= 0.0f)
        m_MoveSpeed = m_Config.ChaseSpeed;
}

void EnemyAI::UpdateFacing()
{
    if (m_State == EnemyState::Dead || m_State == EnemyState::Stunned)
        return;

    if (m_Config.FaceTarget && m_Target != nullptr &&
        (m_State == EnemyState::Chase || m_State == EnemyState::Attack))
    {
        float dx = m_Target->GetPosition().x - m_GameObject->GetPosition().x;
        if (fabsf(dx) > 0.01f)
            m_Facing = (dx > 0.0f) ? 1.0f : -1.0f;
        return;
    }

    if (fabsf(m_MoveDirection.x) > 0.01f)
        m_Facing = (m_MoveDirection.x > 0.0f) ? 1.0f : -1.0f;
}

float EnemyAI::HorizontalDistanceToTarget() const
{
    if (m_Target == nullptr)
        return FLT_MAX;

    return fabsf(m_Target->GetPosition().x - m_GameObject->GetPosition().x);
}

bool EnemyAI::TargetDetected(float Distance) const
{
    if (m_Target == nullptr)
        return false;

    // An enemy that can neither chase nor attack has no reason to look.
    if (!m_Config.CanChase && !m_Config.CanAttack)
        return false;

    bool engaged = (m_State == EnemyState::Chase || m_State == EnemyState::Attack);
    float range = (engaged && m_Config.LoseRange > 0.0f)
        ? m_Config.LoseRange
        : m_Config.DetectRange;

    if (Distance > range)
        return false;

    float dy = fabsf(m_Target->GetPosition().y - m_GameObject->GetPosition().y);
    return dy <= m_Config.DetectHeight;
}

Vector3 EnemyAI::SeparationBias() const
{
    Vector3 bias{ 0.0f, 0.0f, 0.0f };

    if (m_Config.SeparationRadius <= 0.0f || m_Config.SeparationStrength <= 0.0f)
        return bias;

    Vector3 position = m_GameObject->GetPosition();

    for (EnemyAI* other : s_Instances)
    {
        if (other == this || other->m_State == EnemyState::Dead)
            continue;

        Vector3 otherPosition = other->m_GameObject->GetPosition();
        float dx = position.x - otherPosition.x;
        float dy = position.y - otherPosition.y;
        float distance = sqrtf(dx * dx + dy * dy);

        if (distance >= m_Config.SeparationRadius)
            continue;

        float push = (1.0f - distance / m_Config.SeparationRadius) * m_Config.SeparationStrength;

        // 2.5D: everything shares the Z=0 plane, so spread along X only and
        // let the pair settle side by side rather than on top of each other.
        if (distance < 0.0001f)
            bias.x += (this < other) ? -push : push; // exactly overlapping - break the tie
        else
            bias.x += (dx / distance) * push;
    }

    return bias;
}

bool EnemyAI::ConsumeAttack()
{
    if (!m_AttackRequested)
        return false;

    m_AttackRequested = false;
    return true;
}

void EnemyAI::OnDamaged()
{
    Stun(m_Config.StunTime);
}

void EnemyAI::Stun(float Time)
{
    if (m_State == EnemyState::Dead)
        return;

    m_StunTimer = Time;
    SetState(EnemyState::Stunned);

    m_MoveDirection = { 0.0f, 0.0f, 0.0f };
    m_MoveSpeed = 0.0f;
}

void EnemyAI::OnDeath()
{
    SetState(EnemyState::Dead);

    m_MoveDirection = { 0.0f, 0.0f, 0.0f };
    m_MoveSpeed = 0.0f;
    m_AttackRequested = false;
}