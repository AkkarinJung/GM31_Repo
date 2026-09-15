#include "main.h"
#include "renderer.h"
#include <float.h>

#include "EnemyAI.h"

#include "GameObject.h"
#include "manager.h"
#include "Player.h"
#include "MeshField.h"
#include "Collision.h"
#include "Stats.h"

namespace
{
    // Same fixed step the rest of the game runs on.
    const float DELTA_TIME = 1.0f / 60.0f;
}

std::vector<EnemyAI*> EnemyAI::s_Instances;
EnemyAI* EnemyAI::s_AttackToken[EnemyAI::ATTACK_TOKEN_SLOTS] = { nullptr, nullptr };
float EnemyAI::s_TokenCooldown[EnemyAI::ATTACK_TOKEN_SLOTS] = { 0.0f, 0.0f };

namespace
{
    // The beat between one enemy finishing its swing and the next being
    // allowed to start. Without a gap the token just hands straight on and a
    // crowd still produces an unbroken stream of attacks.
    const float TOKEN_HANDOVER_GAP = 0.45f;

    // How often an enemy that has not found its target (or, for a flier, the
    // ground) looks for it again. Manager::GetGameObj walks every object in
    // the scene and dynamic_casts each one, and the scenery alone runs to
    // hundreds, so this is not a per-frame question.
    const float TARGET_SCAN_INTERVAL = 0.25f;

    // "Blocked" means the body got less than this fraction of the movement
    // the AI asked for. Written as a fraction rather than as a fixed distance
    // so it scales with speed - an enemy easing up to full speed is not stuck,
    // it is accelerating.
    const float BLOCKED_PROGRESS_RATIO = 0.25f;

    // How long a searching enemy holds each direction while looking around.
    const float SEARCH_LOOK_PERIOD = 0.6f;

    // How much closer a chase has to get before it counts as progress. Noise
    // around a stopping distance is not progress.
    const float CHASE_PROGRESS_EPSILON = 0.05f;

    // Below this the enemy is simply standing still.
    const float MOVE_EPSILON = 0.001f;

    float Random01()
    {
        return (float)rand() / (float)RAND_MAX;
    }

    // Move Current towards Target by at most MaxStep.
    float MoveTowards(float Current, float Target, float MaxStep)
    {
        float delta = Target - Current;

        if (delta > MaxStep)  delta = MaxStep;
        if (delta < -MaxStep) delta = -MaxStep;

        return Current + delta;
    }

    float Clamp(float Value, float Low, float High)
    {
        if (Value < Low)  return Low;
        if (Value > High) return High;
        return Value;
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

    // Heavy and unhurried. It never charges, so it has no reason to snap into
    // motion either.
    config.Acceleration = 8.0f;
    config.Deceleration = 12.0f;
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

    // It commits. A charge that takes a quarter of a second to wind up to
    // speed is still readable, and the brake is what keeps it from sliding
    // through its own stopping distance and into the player.
    config.Acceleration = 14.0f;
    config.Deceleration = 20.0f;

    // Hunting. Slower than the charge on purpose - a walker that has lost you
    // should look like it is looking, not like it is still coming.
    config.SearchSpeed = 2.0f;
    config.SearchTime = 2.5f;
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

    // It fires across the whole width of its zone, so it is the one type most
    // likely to have scenery drift between it and the player. A short grace
    // stops a single crate corner from interrupting a volley, and anything
    // longer would have it firing blind into a wall.
    config.LoseSightGrace = 0.5f;
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
    config.HoverGain = 2.2f;      // == ChaseSpeed: the hover it has always had
    config.SeparationRadius = 1.6f;
    config.SeparationStrength = 1.2f;

    // It flies over the crates, so nothing on the ground can block it and it
    // has the height to see past most of the scenery.
    config.VisionHalfAngle = 1.3f;
    config.RequireLineOfSight = false;
    config.BlockedGiveUpTime = 99999.0f;
    config.EyeHeight = 0.0f;   // it already hovers well above the ground

    // Light. It drifts rather than charges, so it changes speed gently at
    // both ends - a flier that snapped to full speed would read as a dart.
    config.Acceleration = 7.0f;
    config.Deceleration = 9.0f;
    config.SearchSpeed = 1.8f;
    return config;
}

void EnemyAI::Init()
{
    s_Instances.push_back(this);

    // Roll from whatever config this component starts with, so an AI that is
    // never configured still behaves. The spawner's Configure() re-rolls.
    Configure(m_Config);
}

void EnemyAI::Configure(const EnemyAIConfig& Config)
{
    m_Config = Config;

    // Rolled HERE rather than in Init(). Init() runs the moment the component
    // is added, which is before the spawner has had any chance to configure
    // it - so anything rolled from the config in Init() was rolled from the
    // default config, not from this enemy's own type. It only ever worked
    // because no preset happened to override the reaction range.
    //
    // Rolled once per configure, not per contact: re-rolling every time the
    // enemy spots something would average out and the group would sync up.
    m_ReactionTime = m_Config.ReactionMin +
        (m_Config.ReactionMax - m_Config.ReactionMin) * Random01();
    m_ReactionTimer = m_ReactionTime;

    // Stagger the first line-of-sight test. A row of enemies built on one
    // frame would otherwise run every segment test on the same frame, for the
    // whole level.
    m_PerceptionTimer = m_Config.PerceptionInterval * Random01();
}

void EnemyAI::SetTarget(GameObject* Target)
{
    m_Target = Target;

    // Cached with the target. Every frame the AI needs to know whether the
    // thing it is fighting is still alive, and that question should not cost
    // a walk of the target's component list.
    m_TargetStats = (Target != nullptr) ? Target->GetGameComponent<Stats>() : nullptr;

    m_HasLastKnown = false;
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

    // Ticked before the Dead check, and by exactly one instance: the token gaps
    // are global, and if the enemy that happens to be first in the list is the
    // one that just died, an early return here would freeze them and stall
    // every other enemy's turn to attack.
    if (!s_Instances.empty() && s_Instances[0] == this)
    {
        for (int slot = 0; slot < ATTACK_TOKEN_SLOTS; slot++)
        {
            if (s_TokenCooldown[slot] > 0.0f)
                s_TokenCooldown[slot] -= DELTA_TIME;
        }
    }

    if (m_State == EnemyState::Dead)
        return;

    UpdateTarget();

    m_StateTime += DELTA_TIME;
    m_PerceptionTimer -= DELTA_TIME;

    if (m_AttackCooldownTimer > 0.0f)
        m_AttackCooldownTimer -= DELTA_TIME;

    if (m_StunImmunityTimer > 0.0f)
        m_StunImmunityTimer -= DELTA_TIME;

    if (m_DisengageTimer > 0.0f)
        m_DisengageTimer -= DELTA_TIME;

    UpdateBlocked();

    DecideState();
    Steer();
}

// Scene singletons. Found once and kept - but only asked for a few times a
// second until then, because the search is a dynamic_cast over every object
// in the scene and the scenery alone runs to hundreds.
void EnemyAI::UpdateTarget()
{
    bool needTarget = (m_Target == nullptr);
    bool needField = (m_Config.Flying && m_MeshField == nullptr);

    if (!needTarget && !needField)
        return;

    m_TargetScanTimer -= DELTA_TIME;
    if (m_TargetScanTimer > 0.0f)
        return;

    m_TargetScanTimer = TARGET_SCAN_INTERVAL;

    if (needTarget)
        SetTarget(Manager::GetGameObj<Player>());

    if (needField)
        m_MeshField = Manager::GetGameObj<MeshField>();
}

// Am I getting anywhere? This compares what was ASKED for last frame with what
// the body actually did, which is the only way the AI can find out that a
// crate is in the way - it does no collision of its own. A grounded enemy
// cannot climb, so a chase that stops making progress is a chase that is never
// going to end.
void EnemyAI::UpdateBlocked()
{
    float x = m_GameObject->GetPosition().x;

    bool moving = (m_State == EnemyState::Chase || m_State == EnemyState::Search);
    float asked = fabsf(m_MoveDirection.x) * m_MoveSpeed * DELTA_TIME;

    // Measured against what it asked for rather than against a fixed
    // distance, so an enemy still winding up to speed is not mistaken for one
    // pressed against a wall.
    if (moving && asked > MOVE_EPSILON &&
        fabsf(x - m_LastX) < asked * BLOCKED_PROGRESS_RATIO)
    {
        m_BlockedTimer += DELTA_TIME;

        if (m_BlockedTimer >= m_Config.BlockedGiveUpTime)
        {
            m_BlockedTimer = 0.0f;
            Disengage(m_Config.DisengageTime);
        }
    }
    else
    {
        m_BlockedTimer = 0.0f;
    }

    m_LastX = x;
}

void EnemyAI::SetState(EnemyState State)
{
    if (m_State == State)
        return;

    m_State = State;
    m_StateTime = 0.0f;

    // The stop/start band is a chase-only idea; leaving it set would have the
    // enemy refuse to move on its next chase until the player backed off.
    if (State != EnemyState::Chase)
        m_HoldingPosition = false;

    // Every chase gets a fresh progress budget. -1 means "not seeded yet":
    // the first update of the chase records whatever distance it starts at.
    if (State == EnemyState::Chase)
    {
        m_BestChaseDistance = -1.0f;
        m_NoProgressTimer = 0.0f;
    }
}

// A chase is working if it is getting closer or landing swings. This is the
// companion to UpdateBlocked: that one catches an enemy leaning on a crate,
// this one catches an enemy that has stopped moving altogether and is simply
// never going to reach what it is chasing.
bool EnemyAI::ChaseGaveUp(float Distance)
{
    if (m_State != EnemyState::Chase)
    {
        m_NoProgressTimer = 0.0f;
        m_BestChaseDistance = -1.0f;
        return false;
    }

    if (m_BestChaseDistance < 0.0f || Distance < m_BestChaseDistance - CHASE_PROGRESS_EPSILON)
    {
        m_BestChaseDistance = Distance;
        m_NoProgressTimer = 0.0f;
        return false;
    }

    m_NoProgressTimer += DELTA_TIME;

    return m_NoProgressTimer >= m_Config.ChaseGiveUpTime;
}

// Forget the target. One place, because the gave-up-on-a-crate path and the
// gave-up-searching path have to leave exactly the same state behind, and they
// used not to.
void EnemyAI::Disengage(float IgnoreTime)
{
    m_Aware = false;
    m_Alerted = false;
    m_ReactionTimer = m_ReactionTime;
    m_HasLastKnown = false;
    m_LostSightTimer = 0.0f;
    m_SearchTimer = 0.0f;

    ReleaseAttackToken();

    // Only ever extends the window. This runs every frame an idle enemy sees
    // nothing, so assigning unconditionally would wipe the window the blocked
    // path had just set and put the enemy straight back onto the crate.
    if (IgnoreTime > 0.0f)
        m_DisengageTimer = IgnoreTime;
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

    // Checked before the fix, so a chase that has stalled ends this frame
    // rather than being renewed by a target that is still perfectly visible.
    // Disengage clears the last known position too, so what follows sends the
    // enemy back to its beat instead of into a search for a player it can see.
    if (ChaseGaveUp(distance))
        Disengage(m_Config.DisengageTime);

    bool fix = HasFixOnTarget(distance);

    // Reaction. Having a fix on the player is not the same as having reacted
    // to them: the enemy has to hold the contact for its own roll of a
    // fraction of a second before it may act on it. Everything downstream
    // uses "detected", so a fresh contact cannot chase or swing during that
    // window.
    if (fix && !m_Aware)
    {
        m_ReactionTimer -= DELTA_TIME;
        if (m_ReactionTimer <= 0.0f)
            m_Aware = true;
    }

    bool detected = fix && m_Aware;

    if (detected)
    {
        // Worth remembering. This is the spot a search will head for, and it
        // is only ever written while the enemy genuinely has the target.
        m_LastKnownPosition = m_Target->GetPosition();
        m_HasLastKnown = true;
        m_SearchTimer = 0.0f;
    }
    else if (!fix)
    {
        // Genuinely lost, as opposed to merely still reacting.
        //
        // This used to read "else", which meant every frame of the reaction
        // window - when the enemy HAS a fix but has not finished reacting to
        // it, so "detected" is still false - fell down here and called
        // Disengage, and Disengage resets m_ReactionTimer. The timer was
        // decremented by one frame and put straight back, so it could never
        // reach zero and m_Aware could never be set by looking at anything.
        // The only thing that could still set it was Alert(), which is to say
        // being hit: enemies ignored the player completely until the player
        // swung first, and then fought perfectly normally.
        ReleaseAttackToken(); // lost them - do not sit on the token

        // Something it was actually fighting is worth going to look for.
        // Snapping straight back to a patrol beat the instant the player
        // steps behind a crate is the single clearest tell that an enemy is
        // running on a distance check rather than on knowing anything.
        bool wasEngaged = (m_State == EnemyState::Chase || m_State == EnemyState::Attack);

        if (wasEngaged && m_Config.CanChase && m_HasLastKnown && TargetAlive())
        {
            m_SearchTimer = m_Config.SearchTime;
            SetState(EnemyState::Search);
            return; // stays aware while hunting, so a re-sighting is instant
        }

        if (m_State == EnemyState::Search)
        {
            m_SearchTimer -= DELTA_TIME;
            if (m_SearchTimer > 0.0f)
                return;
        }

        // Nothing left to go on. No ignore window: this is simply not knowing
        // where the player is, not refusing to look.
        Disengage(0.0f);
    }

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

            // Landing swings is the whole point of a chase, so an enemy
            // trading blows at its stopping distance is not stalled - even
            // though it is not getting any closer.
            m_NoProgressTimer = 0.0f;

            // Spread, so two enemies that started their fight on the same
            // frame do not stay on the same beat for the rest of it.
            float spread = 1.0f + (Random01() * 2.0f - 1.0f) * m_Config.AttackCooldownVariance;
            m_AttackCooldownTimer = m_Config.AttackCooldown * spread;
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

// Turns the current state into a velocity the owner can apply directly.
//
// Everything here works in units per second and the smoothing happens on the
// velocity itself, so a start, a stop and a turn-around are all the same
// operation. The direction/speed pair the owner reads is derived at the end -
// direction is always unit length, which is what stops a nudged enemy
// travelling faster than its own ChaseSpeed.
void EnemyAI::Steer()
{
    float desiredVelocityX = 0.0f;
    float topSpeed = m_Config.ChaseSpeed;

    bool canMove = (m_State != EnemyState::Stunned &&
                    m_State != EnemyState::Attack &&
                    m_State != EnemyState::Dead);

    if (canMove)
    {
        switch (m_State)
        {
        case EnemyState::Patrol:
        {
            topSpeed = m_Config.PatrolSpeed;

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

            desiredVelocityX = m_PatrolDirection * m_Config.PatrolSpeed;
            break;
        }

        case EnemyState::Chase:
        {
            if (m_Target == nullptr)
                break;

            float dx = m_Target->GetPosition().x - m_GameObject->GetPosition().x;
            float gap = fabsf(dx);

            // Stop close, start far. Two thresholds rather than one: with a
            // single StopDistance a player standing exactly on it flipped the
            // enemy between "walk" and "stand" every frame, which is the
            // vibrating-around-the-player bug.
            if (m_HoldingPosition)
            {
                if (gap > m_Config.StopDistance + m_Config.StopBand)
                    m_HoldingPosition = false;
            }
            else if (gap <= m_Config.StopDistance)
            {
                m_HoldingPosition = true;
            }

            if (!m_HoldingPosition)
                desiredVelocityX = (dx > 0.0f ? 1.0f : -1.0f) * m_Config.ChaseSpeed;

            break;
        }

        case EnemyState::Search:
        {
            topSpeed = m_Config.SearchSpeed;

            if (!m_HasLastKnown)
                break;

            float dx = m_LastKnownPosition.x - m_GameObject->GetPosition().x;

            // Arrived: stand and look. UpdateFacing sweeps it left and right
            // from here, which is the whole readable part of a search.
            if (fabsf(dx) > m_Config.SearchArriveDistance)
                desiredVelocityX = (dx > 0.0f ? 1.0f : -1.0f) * m_Config.SearchSpeed;

            break;
        }

        default:
            break;
        }

        // Spacing, added on top of whatever the state wanted - so an enemy
        // that has stopped closing in still fans out instead of stacking into
        // its neighbour's silhouette. Scaled to a real speed here; the bias
        // itself is a unitless 0..SeparationStrength.
        desiredVelocityX += SeparationBias() * m_Config.ChaseSpeed;
    }

    // Accelerate towards what it wants. Slowing down and turning round both
    // use the brake, which is the higher of the two rates - an enemy that
    // reversed as lazily as it started would read as sliding on ice.
    bool braking = (fabsf(desiredVelocityX) < fabsf(m_VelocityX)) ||
                   (desiredVelocityX * m_VelocityX < 0.0f);

    float rate = braking ? m_Config.Deceleration : m_Config.Acceleration;

    m_VelocityX = MoveTowards(m_VelocityX, desiredVelocityX, rate * DELTA_TIME);
    m_VelocityX = Clamp(m_VelocityX, -topSpeed, topSpeed);

    float velocityY = VerticalVelocity(); // 0 unless it flies

    // Split back into the direction/speed pair the owner applies. Doing it
    // here rather than letting callers multiply a ragged direction by a speed
    // is what guarantees a crowded enemy cannot outrun a lone one.
    float speed = sqrtf(m_VelocityX * m_VelocityX + velocityY * velocityY);

    if (speed > MOVE_EPSILON)
    {
        m_MoveDirection = Vector3(m_VelocityX / speed, velocityY / speed, 0.0f);
        m_MoveSpeed = speed;
    }
    else
    {
        m_MoveDirection = Vector3(0.0f, 0.0f, 0.0f);
        m_MoveSpeed = 0.0f;
    }

    UpdateFacing();
}

// A flier holds its own altitude, in every state - including while stunned or
// mid-swing, because the alternative is dropping out of the sky.
float EnemyAI::VerticalVelocity()
{
    if (!m_Config.Flying || m_State == EnemyState::Dead)
        return 0.0f;

    m_BobTime += DELTA_TIME;

    float baseY;

    // Station-keeping over the target while it is fighting one, over the
    // ground otherwise.
    if ((m_State == EnemyState::Chase || m_State == EnemyState::Attack) && m_Target != nullptr)
    {
        baseY = m_Target->GetPosition().y + m_Config.HoverHeight;
    }
    else
    {
        float ground = (m_MeshField != nullptr)
            ? m_MeshField->GetHeight(m_GameObject->GetPosition())
            : m_Home.y;

        baseY = ground + m_Config.HoverHeight;
    }

    float desiredY = baseY + sinf(m_BobTime * m_Config.BobSpeed) * m_Config.BobAmplitude;

    // Proportional: it closes a large gap at its top speed and eases in over
    // the last stretch, so arriving at hover height does not snap.
    float error = desiredY - m_GameObject->GetPosition().y;

    return Clamp(error * m_Config.HoverGain, -m_Config.ChaseSpeed, m_Config.ChaseSpeed);
}

void EnemyAI::UpdateFacing()
{
    if (m_State == EnemyState::Dead || m_State == EnemyState::Stunned)
        return;

    // Hunting. It does NOT face the target - it cannot see it, and turning to
    // stare through a crate at something it is supposed to have lost is the
    // one thing that would give the whole search away. It faces where it is
    // going, and sweeps once it gets there.
    if (m_State == EnemyState::Search)
    {
        if (fabsf(m_MoveDirection.x) > 0.01f)
        {
            m_Facing = (m_MoveDirection.x > 0.0f) ? 1.0f : -1.0f;
        }
        else
        {
            float sweep = fmodf(m_SearchTimer, SEARCH_LOOK_PERIOD * 2.0f);
            m_Facing = (sweep < SEARCH_LOOK_PERIOD) ? 1.0f : -1.0f;
        }
        return;
    }

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

// A target with no HP left is not a target. Without this the whole stage kept
// chasing, swinging at and shouting about a player who had already died -
// hurt sounds over the death animation, and a crowd still pressing in on a
// corpse.
bool EnemyAI::TargetAlive() const
{
    if (m_Target == nullptr)
        return false;

    return m_TargetStats == nullptr || !m_TargetStats->IsDead();
}

// Everything the enemy knows about its target this frame, in one answer.
// Cheap tests first: the cone is arithmetic, the wall test walks the map.
bool EnemyAI::HasFixOnTarget(float Distance)
{
    if (!TargetAlive())
        return false;

    // An enemy that can neither chase nor attack has no reason to look.
    if (!m_Config.CanChase && !m_Config.CanAttack)
        return false;

    // It gave up on this target a moment ago - let it walk away rather than
    // re-acquiring on the next frame and going straight back to shoving the
    // crate it could not get past.
    if (m_DisengageTimer > 0.0f)
        return false;

    bool engaged = (m_State == EnemyState::Chase ||
                    m_State == EnemyState::Attack ||
                    m_State == EnemyState::Search);

    float range = (engaged && m_Config.LoseRange > 0.0f)
        ? m_Config.LoseRange
        : m_Config.DetectRange;

    if (Distance > range)
        return false;

    float dy = fabsf(m_Target->GetPosition().y - m_GameObject->GetPosition().y);
    if (dy > m_Config.DetectHeight)
        return false;

    // Already fighting, or just been hit: it knows where the player is, and
    // losing track because the player stepped behind it mid-fight looks far
    // worse than seeing a little too much.
    bool knows = engaged || m_Alerted;

    if (!knows && !ConeContainsTarget())
        return false;

    // The wall always counts, though - including mid-fight, which it did not
    // used to. An engaged enemy skipped this test entirely, so a turret
    // happily kept firing into the crate the player was standing behind and a
    // walker tracked them through it for the whole nine units of its lose
    // range.
    //
    // Losing sight does not wipe the fix on the spot: it starts a clock. That
    // grace is what keeps a chase alive as the player runs past scenery,
    // while still ending it when they actually break away.
    if (HasLineOfSight())
    {
        m_LostSightTimer = 0.0f;
        return true;
    }

    m_LostSightTimer += DELTA_TIME;

    if (!knows)
        return false;

    return m_LostSightTimer < m_Config.LoseSightGrace;
}

// The cone, in the plane the game is played in. Cheap enough to run every
// frame, and it is the responsive half of perception.
bool EnemyAI::ConeContainsTarget() const
{
    Vector3 self = m_GameObject->GetPosition();
    Vector3 target = m_Target->GetPosition();

    float dx = target.x - self.x;
    float dy = target.y - self.y;
    float length = sqrtf(dx * dx + dy * dy);

    if (length <= 0.0001f)
        return true; // right on top of it - any facing will do

    // m_Facing is +/-1 along X, so the dot product with it is just dx.
    float cosAngle = (dx * m_Facing) / length;

    return cosAngle >= cosf(m_Config.VisionHalfAngle);
}

// The wall. This is the expensive half - it walks every solid in the map - so
// it runs on PerceptionInterval and hands back the last answer in between.
// 0.1s is well inside a player's own reaction time, and with fifteen enemies
// on a stage it is the difference between fifteen segment tests a frame and
// roughly two.
bool EnemyAI::HasLineOfSight()
{
    if (!m_Config.RequireLineOfSight)
        return true;

    if (m_PerceptionTimer > 0.0f)
        return m_LineOfSight;

    // Jittered, so enemies that came into range together do not stay in
    // lockstep and spike the same frame forever after.
    m_PerceptionTimer = m_Config.PerceptionInterval * (0.75f + 0.5f * Random01());

    // Eye to eye, not foot to foot: standing next to a crate would otherwise
    // block the enemy's view of its own feet.
    Vector3 eye = m_GameObject->GetPosition();
    eye.y += m_Config.EyeHeight;

    Vector3 aim = m_Target->GetPosition();
    aim.y += m_Config.EyeHeight;

    m_LineOfSight = !Collision::SegmentBlocked(eye, aim, Collision::GatherSolids());

    return m_LineOfSight;
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
    int slot = TokenSlot();

    if (s_AttackToken[slot] == this)
        return true;

    if (s_AttackToken[slot] != nullptr)
        return false;

    if (s_TokenCooldown[slot] > 0.0f)
        return false;

    s_AttackToken[slot] = this;
    return true;
}

// Scans both slots rather than only this enemy's own. An enemy that was
// reconfigured between taking a token and giving it back would otherwise leak
// the one it actually holds, and a leaked token is permanent: nothing else of
// that kind would ever be allowed to attack again.
void EnemyAI::ReleaseAttackToken()
{
    for (int slot = 0; slot < ATTACK_TOKEN_SLOTS; slot++)
    {
        if (s_AttackToken[slot] != this)
            continue;

        s_AttackToken[slot] = nullptr;
        s_TokenCooldown[slot] = TOKEN_HANDOVER_GAP;
    }
}

void EnemyAI::Alert()
{
    m_Alerted = true;
    m_Aware = true;
    m_ReactionTimer = 0.0f;
    m_DisengageTimer = 0.0f;

    // Being hit is a fix on the target in its own right, and a better one
    // than sight: without this an enemy struck from behind a crate had
    // nowhere to search, so it simply stood there.
    if (m_Target != nullptr)
    {
        m_LastKnownPosition = m_Target->GetPosition();
        m_HasLastKnown = true;
        m_LostSightTimer = 0.0f;
    }
}

// A unitless nudge along X, 0..SeparationStrength. Steering, not collision -
// the owner still resolves real overlap.
float EnemyAI::SeparationBias() const
{
    if (m_Config.SeparationRadius <= 0.0f || m_Config.SeparationStrength <= 0.0f)
        return 0.0f;

    Vector3 position = m_GameObject->GetPosition();
    float bias = 0.0f;

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
            bias += (this < other) ? -push : push; // exactly overlapping - break the tie
        else
            bias += (dx / distance) * push;
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

    // The smoothed velocity is cleared too, not just the output. Leaving it
    // set would have the enemy resume its charge from full speed the instant
    // the stun ended, which is not what being staggered looks like.
    m_VelocityX = 0.0f;
    m_MoveDirection = { 0.0f, 0.0f, 0.0f };
    m_MoveSpeed = 0.0f;
}

void EnemyAI::OnDeath()
{
    SetState(EnemyState::Dead);
    ReleaseAttackToken();

    m_VelocityX = 0.0f;
    m_MoveDirection = { 0.0f, 0.0f, 0.0f };
    m_MoveSpeed = 0.0f;
    m_AttackRequested = false;
}

const char* EnemyAI::GetStateName() const
{
    switch (m_State)
    {
    case EnemyState::Idle:    return "Idle";
    case EnemyState::Patrol:  return "Patrol";
    case EnemyState::Chase:   return "Chase";
    case EnemyState::Search:  return "Search";
    case EnemyState::Attack:  return "Attack";
    case EnemyState::Stunned: return "Stunned";
    case EnemyState::Dead:    return "Dead";
    }

    return "Unknown";
}
