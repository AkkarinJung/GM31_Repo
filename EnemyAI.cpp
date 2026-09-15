#include "main.h"
#include "renderer.h"
#include <float.h>

#include "EnemyAI.h"

#include "GameObject.h"
#include "manager.h"
#include "Player.h"
#include "MeshField.h"
#include "Collision.h"

namespace
{
    // Same fixed step the rest of the game runs on.
    const float DELTA_TIME = 1.0f / 60.0f;
}

std::vector<EnemyAI*> EnemyAI::s_Instances;
EnemyAI* EnemyAI::s_AttackToken = nullptr;
float EnemyAI::s_TokenCooldown = 0.0f;

namespace
{
    // The beat between one enemy finishing its swing and the next being
    // allowed to start. Without a gap the token just hands straight on and a
    // crowd still produces an unbroken stream of attacks.
    const float TOKEN_HANDOVER_GAP = 0.45f;

    float Random01()
    {
        return (float)rand() / (float)RAND_MAX;
    }
}

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

    // It barely looks up from its beat, so give it a narrow cone. Being hit
    // still turns it round - see EnemyAI::Alert.
    config.VisionHalfAngle = 0.9f;
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

    // The fighter of the three: a wide cone and the full crowd behaviour.
    config.VisionHalfAngle = 1.15f;   // ~66 degrees each side
    config.NeedsAttackToken = true;
    return config;
}

EnemyAIConfig EnemyAIConfig::Turret()
{
    EnemyAIConfig config;
    // The only one of the four that throws a wave instead of swinging, and
    // the reason the split exists: it cannot move at all, so range is the
    // only thing it has. The three that CAN move close to arm's length and
    // swing, which is what their tuned ranges below already describe.
    //
    // The ranges here are firing distances, not reach. They have to be wide:
    // a wave launched at arm's length is just a slower melee hit, with no
    // flight for the player to read.
    config.CanAttack = true;
    config.RangedAttack = true;
    config.AttackRange = 9.0f;
    config.AttackHeight = 3.5f;
    config.AttackCooldown = 1.8f;
    config.AttackDuration = 0.75f;
    config.DetectRange = 10.0f;
    config.LoseRange = 13.0f;

    // It cannot move, so it can never be "blocked" and must never give up -
    // and a thing that only guards its own spot gets to watch all round it.
    config.VisionHalfAngle = 3.15f;       // effectively 360
    config.BlockedGiveUpTime = 99999.0f;
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

    // It flies over the crates, so nothing on the ground can block it and it
    // has the height to see past most of the scenery.
    config.VisionHalfAngle = 1.3f;
    config.RequireLineOfSight = false;
    config.BlockedGiveUpTime = 99999.0f;
    config.EyeHeight = 0.0f;   // it already hovers well above the ground
    return config;
}

void EnemyAI::Init()
{
    s_Instances.push_back(this);

    // Rolled once, here, so this enemy keeps its own reaction for its whole
    // life - re-rolling every time it spots something would average out and
    // the group would sync up again.
    m_ReactionTime = m_Config.ReactionMin +
        (m_Config.ReactionMax - m_Config.ReactionMin) * Random01();
    m_ReactionTimer = m_ReactionTime;
}

void EnemyAI::Uninit()
{
    // Never leave the token held by a dead enemy - nothing else would ever be
    // allowed to attack again.
    ReleaseAttackToken();

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

    // Ticked before the Dead check, and by exactly one instance: the token gap
    // is global, and if the enemy that happens to be first in the list is the
    // one that just died, an early return here would freeze the gap and stall
    // every other enemy's turn to attack.
    if (!s_Instances.empty() && s_Instances[0] == this && s_TokenCooldown > 0.0f)
        s_TokenCooldown -= DELTA_TIME;

    if (m_State == EnemyState::Dead)
        return;

    if (m_Target == nullptr)
        m_Target = Manager::GetGameObj<Player>();

    m_StateTime += DELTA_TIME;

    if (m_AttackCooldownTimer > 0.0f)
        m_AttackCooldownTimer -= DELTA_TIME;

    if (m_StunImmunityTimer > 0.0f)
        m_StunImmunityTimer -= DELTA_TIME;

    if (m_DisengageTimer > 0.0f)
        m_DisengageTimer -= DELTA_TIME;

    // Am I getting anywhere? This compares what was ASKED for last frame with
    // what the body actually did, which is the only way the AI can find out
    // that a crate is in the way - it does no collision of its own. A grounded
    // enemy cannot climb, so a chase that stops making progress is a chase
    // that is never going to end.
    float x = m_GameObject->GetPosition().x;
    bool pushing = (m_State == EnemyState::Chase) &&
        (m_MoveSpeed > 0.0f) && (fabsf(m_MoveDirection.x) > 0.01f);

    if (pushing && fabsf(x - m_LastX) < 0.005f)
    {
        m_BlockedTimer += DELTA_TIME;

        if (m_BlockedTimer >= m_Config.BlockedGiveUpTime)
        {
            m_BlockedTimer = 0.0f;
            m_DisengageTimer = m_Config.DisengageTime;
            m_Aware = false;
            m_Alerted = false;
            m_ReactionTimer = m_ReactionTime;
            ReleaseAttackToken();
        }
    }
    else
    {
        m_BlockedTimer = 0.0f;
    }

    m_LastX = x;

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

        ReleaseAttackToken(); // its turn is over - let the next one in
        SetState(EnemyState::Idle);
    }

    float distance = HorizontalDistanceToTarget();
    bool sensed = TargetDetected(distance);

    // Reaction. Sensing the player is not the same as having reacted to them:
    // the enemy has to have held the contact for its own roll of a fraction of
    // a second before it may act on it. Everything downstream uses "detected",
    // so a fresh contact cannot chase or swing during that window.
    if (sensed)
    {
        if (!m_Aware)
        {
            m_ReactionTimer -= DELTA_TIME;
            if (m_ReactionTimer <= 0.0f)
                m_Aware = true;
        }
    }
    else
    {
        m_Aware = false;
        m_Alerted = false;
        m_ReactionTimer = m_ReactionTime;
        ReleaseAttackToken(); // lost them - do not sit on the token
    }

    bool detected = sensed && m_Aware;

    // Vertical gap measured exactly the way the swing measures it - see
    // AttackVerticalGap. Deciding on a symmetric fabs() while the swing used
    // an asymmetric band meant the enemy committed to attacks it could not
    // land, burning the whole attack state and cooldown standing frozen.
    float verticalGap = AttackVerticalGap();

    if (m_Config.CanAttack && detected &&
        distance <= m_Config.AttackRange &&
        verticalGap <= m_Config.AttackHeight &&
        m_AttackCooldownTimer <= 0.0f &&
        FacingTarget())
    {
        // Only one of a crowd may swing at a time. Everyone else falls
        // through to Chase below and keeps their spacing.
        if (!m_Config.NeedsAttackToken || TryTakeAttackToken())
        {
            SetState(EnemyState::Attack);
            m_AttackRequested = true;
            m_AttackCooldownTimer = m_Config.AttackCooldown;
            return;
        }
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

    // Normalise. Steer adds a separation bias straight onto the direction and
    // UpdateVertical writes into it too, and Enemy::Update then does
    // velocity = direction * speed - so an enemy pressed against a neighbour
    // was travelling at up to twice its own ChaseSpeed, and a flyer moving
    // diagonally at 1.41x. A crowded enemy outran a lone one and the player
    // could never learn what "an enemy's speed" is.
    //
    // Only shrunk, never grown: a small nudge should stay a small nudge.
    float length = sqrtf(m_MoveDirection.x * m_MoveDirection.x +
                         m_MoveDirection.y * m_MoveDirection.y);

    if (length > 1.0f)
    {
        m_MoveDirection.x /= length;
        m_MoveDirection.y /= length;
    }
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

    // Turn to face the target whenever it is engaged with one - including
    // while standing perfectly still.
    //
    // This used to be gated on Chase or Attack alone. A Turret cannot move, so
    // it never enters Chase and never turned: it sat facing whichever way it
    // spawned for its whole life. That was merely odd until a swing started
    // requiring the enemy to be facing its target (see FacingTarget), at which
    // point a turret could never attack anything that walked up on its other
    // side at all.
    bool engaged = (m_State == EnemyState::Chase || m_State == EnemyState::Attack) ||
                   (m_Aware && m_Config.CanAttack);

    // Being hit turns anything round, whatever its config says. A Patroller
    // has FaceTarget off on purpose - it walks its beat and does not care
    // about you - but "hit it in the back and it never once looks at you"
    // reads as broken rather than as indifferent.
    bool mustFace = m_Config.FaceTarget || m_Alerted;

    if (mustFace && m_Target != nullptr && engaged)
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

    // It gave up on this target a moment ago - let it walk away rather than
    // re-acquiring on the next frame and going straight back to shoving the
    // crate it could not get past.
    if (m_DisengageTimer > 0.0f)
        return false;

    bool engaged = (m_State == EnemyState::Chase || m_State == EnemyState::Attack);
    float range = (engaged && m_Config.LoseRange > 0.0f)
        ? m_Config.LoseRange
        : m_Config.DetectRange;

    if (Distance > range)
        return false;

    float dy = fabsf(m_Target->GetPosition().y - m_GameObject->GetPosition().y);
    if (dy > m_Config.DetectHeight)
        return false;

    // Already fighting, or just been hit: it knows where the player is and the
    // cone stops applying. Losing track because the player stepped behind it
    // mid-fight looks far worse than seeing a little too much.
    if (engaged || m_Alerted)
        return true;

    return CanSeeTarget();
}

// The cone, and then the wall. Split out because it is the expensive half and
// only worth running once range and height have already passed.
bool EnemyAI::CanSeeTarget() const
{
    Vector3 self = m_GameObject->GetPosition();
    Vector3 target = m_Target->GetPosition();

    float dx = target.x - self.x;
    float dy = target.y - self.y;
    float length = sqrtf(dx * dx + dy * dy);

    if (length > 0.0001f)
    {
        // m_Facing is +/-1 along X, so the dot product with it is just dx.
        float cosAngle = (dx * m_Facing) / length;
        if (cosAngle < cosf(m_Config.VisionHalfAngle))
            return false;
    }

    if (!m_Config.RequireLineOfSight)
        return true;

    // Eye to eye, not foot to foot: standing next to a crate would otherwise
    // block the enemy's view of its own feet.
    Vector3 eye = self;
    eye.y += m_Config.EyeHeight;

    Vector3 aim = target;
    aim.y += m_Config.EyeHeight;

    return !Collision::SegmentBlocked(eye, aim, Collision::GatherSolids());
}

// Written the same way round as Enemy::CanReachTarget, so the decision to
// swing and the swing itself can never disagree about reach.
float EnemyAI::AttackVerticalGap() const
{
    if (m_Target == nullptr)
        return 0.0f;

    float dy = m_GameObject->GetPosition().y - m_Target->GetPosition().y;

    if (dy > m_Config.AttackTargetHeight)
        return dy - m_Config.AttackTargetHeight; // above the target's head
    if (dy < 0.0f)
        return -dy;                              // target is above this enemy

    return 0.0f;
}

// Do not swing at something behind you. The owner turns the body smoothly, so
// without this an enemy could commit to an attack mid-turn and the swing would
// play out pointing the wrong way.
bool EnemyAI::FacingTarget() const
{
    if (m_Target == nullptr)
        return false;

    float dx = m_Target->GetPosition().x - m_GameObject->GetPosition().x;

    if (fabsf(dx) < 0.05f)
        return true; // right on top of it - any facing will do

    return (dx > 0.0f) == (m_Facing > 0.0f);
}

bool EnemyAI::TryTakeAttackToken()
{
    if (s_AttackToken == this)
        return true;

    if (s_AttackToken != nullptr)
        return false;

    if (s_TokenCooldown > 0.0f)
        return false;

    s_AttackToken = this;
    return true;
}

void EnemyAI::ReleaseAttackToken()
{
    if (s_AttackToken != this)
        return;

    s_AttackToken = nullptr;
    s_TokenCooldown = TOKEN_HANDOVER_GAP;
}

void EnemyAI::Alert()
{
    m_Alerted = true;
    m_Aware = true;
    m_ReactionTimer = 0.0f;
    m_DisengageTimer = 0.0f;
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
    // Poise. The damage has already been applied by the caller - this is only
    // about whether the enemy is allowed to STOP and react, and it is not
    // allowed to do that on every hit of a combo. See StunImmunity.
    // Being hit always tells it where you are, even through its blind spot and
    // even if it had just given up - that part is not gated by poise.
    Alert();

    if (m_StunImmunityTimer > 0.0f)
        return;

    m_StunImmunityTimer = m_Config.StunImmunity;
    Stun(m_Config.StunTime);
}

// A parry, and anything else that should stagger the enemy no matter what,
// comes through here rather than through OnDamaged - it ignores the poise
// timer on purpose. Reading a telegraph correctly is the one thing that is
// always supposed to open the enemy up.
void EnemyAI::Stun(float Time)
{
    if (m_State == EnemyState::Dead)
        return;

    m_StunTimer = Time;
    SetState(EnemyState::Stunned);

    // Staggered enemies do not keep their turn to attack.
    ReleaseAttackToken();

    m_MoveDirection = { 0.0f, 0.0f, 0.0f };
    m_MoveSpeed = 0.0f;
}

void EnemyAI::OnDeath()
{
    SetState(EnemyState::Dead);
    ReleaseAttackToken();

    m_MoveDirection = { 0.0f, 0.0f, 0.0f };
    m_MoveSpeed = 0.0f;
    m_AttackRequested = false;
}