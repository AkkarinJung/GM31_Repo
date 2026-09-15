#include "main.h"
#include "renderer.h" // GameObject.h calls Renderer:: inline
#include "EnemyShot.h"

#include "manager.h"
#include "Collision.h"
#include "Enemy.h"
#include "Player.h"
#include "Stats.h"
#include "SlashEffect.h"
#include "SoundEffect.h"

#include <math.h>

// How fast a wave travels. Distance, not time, is what ends it: the shot
// burns out once it has covered the firing range of the enemy that threw it
// (see Fire). The lifetime below is only a backstop against a shot that
// somehow never accumulates distance.
static const float SHOT_SPEED = 11.0f;
static const float SHOT_LIFETIME = 3.0f;

// A little past the firing range, so a wave thrown at the very edge of an
// enemy's reach is not beaten by the player taking one step back during the
// flight. Set it to zero for a hard cut at exactly the range.
static const float SHOT_RANGE_SLACK = 1.0f;

// The wave's own body, for the two things it can hit.
static const float SHOT_RADIUS = 0.30f;      // against crates and walls
static const float HIT_HALF_WIDTH = 0.55f;   // against the player
static const float HIT_HALF_HEIGHT = 0.95f;  // the player's body is 1.8 tall

// Where the player's middle is, measured up from their feet - Player's
// position is at the feet, like every character here.
static const float PLAYER_CENTRE_Y = 0.9f;

// The trail. A fresh SlashEffect every interval, each living a little longer
// than the gap, so two or three overlap and the wave reads as continuous
// rather than as a dotted line.
//
// Re-emitting is what lets this use the player's attack sprite EXACTLY as it
// is. SlashEffect stretches and fades over its own lifetime, so one long
// lived instance would swell and dim across the whole flight; a stream of
// short ones plays that curve the way it was tuned, over and over.
static const float TRAIL_INTERVAL = 0.06f;
static const float TRAIL_LIFETIME = 0.16f;

// Smaller than the player's 1.5 x 1.5 swing - a thrown wave, not a full
// body swing, and it has to be readable as something to dodge rather than
// something that fills the lane.
static const float TRAIL_LENGTH = 0.85f;
static const float TRAIL_THICKNESS = 0.85f;
static const float TRAIL_ALPHA = 0.95f;

void EnemyShot::Init()
{
    m_Layer = 2; // with the effects, not the terrain

    // Shared with the player's slash, and already loaded by Game::Init - this
    // is only here so the first shot of a stage cannot pay for it mid-flight.
    SlashEffect::LoadShared();
}

void EnemyShot::Fire(Enemy* Owner, const Vector3& Position, const Vector3& Direction,
    int Damage, float MaxRange)
{
    m_Owner = Owner;
    m_Position = Position;
    m_Damage = Damage;
    m_Life = SHOT_LIFETIME;

    m_Range = MaxRange + SHOT_RANGE_SLACK;
    m_Travelled = 0.0f;

    Vector3 direction = Direction;
    direction.z = 0.0f; // the play plane is XY - a shot never leaves it

    float length = direction.lenght();

    if (length < 0.0001f)
        direction = Vector3(1.0f, 0.0f, 0.0f);
    else
        direction /= length;

    m_Velocity = direction * SHOT_SPEED;

    // The crescent is drawn already bulging along +X, so pointing it down the
    // flight path is just its angle - no mirroring. A swing needs the
    // hand-authored table in Player::SpawnSlash because an arc has a shape;
    // a wave travelling in a straight line only has a direction.
    m_Roll = atan2f(direction.y, direction.x);

    EmitTrail(); // one on the very first frame, so the shot is never invisible
}

Enemy* EnemyShot::LiveOwner() const
{
    if (m_Owner == nullptr)
        return nullptr;

    // The pointer alone proves nothing - Manager deletes enemies, and this
    // shot can easily outlive the one that fired it. Only a match against
    // the live list makes it safe to touch.
    auto enemies = Manager::GetGameObjs<Enemy>();

    for (auto enemy : enemies)
    {
        if (enemy == m_Owner)
            return enemy;
    }

    return nullptr;
}

void EnemyShot::EmitTrail()
{
    SlashEffect* slash = Manager::AddGameObj<SlashEffect>();

    // The crescent, like every other thing an enemy throws. The sword sheet
    // is the player's swing and nothing else wears it.
    slash->SetStyle(SlashStyle::Crescent);

    slash->Play(m_Position, Vector3(0.0f, 0.0f, m_Roll),
        Vector3(TRAIL_LENGTH, TRAIL_THICKNESS, 1.0f),
        TRAIL_LIFETIME, TRAIL_ALPHA,
        0.0f); // no sweep - the shot's own travel is the movement
}

bool EnemyShot::HitSolid() const
{
    // The same solids the player walks on, so a crate is cover and the map
    // edge hedges stop a stray shot. Enemies already refuse to chase what
    // they cannot see (EnemyAIConfig::RequireLineOfSight) - this is the other
    // half of that: what they cannot see, they cannot shoot through either.
    std::vector<AABB> solids = Collision::GatherSolids();

    for (size_t i = 0; i < solids.size(); i++)
    {
        if (Collision::SphereVsAABB(m_Position, SHOT_RADIUS,
            solids[i].Center, solids[i].HalfSize))
            return true;
    }

    return false;
}

bool EnemyShot::HitPlayer()
{
    Player* player = Manager::GetGameObj<Player>();

    if (player == nullptr)
        return false;

    Vector3 playerCentre = player->GetPosition();
    playerCentre.y += PLAYER_CENTRE_Y;

    // 2.5D, like every other reach test here: one limit across, another up.
    if (fabsf(m_Position.x - playerCentre.x) > HIT_HALF_WIDTH)
        return false;

    if (fabsf(m_Position.y - playerCentre.y) > HIT_HALF_HEIGHT)
        return false;

    // Parried. The answer is the swing's answer: no damage, the shot is
    // gone, and whoever threw it is left open - if it is still alive to be
    // left open.
    Enemy* owner = LiveOwner();

    if (player->TryParry(owner))
    {
        if (owner != nullptr)
            owner->OnShotParried();

        return true;
    }

    Stats* stats = player->GetGameComponent<Stats>();

    if (stats != nullptr)
    {
        stats->TakeDamage(m_Damage);
        SoundEffect::Play(SE::PlayerHurt);
    }

    return true;
}

void EnemyShot::Update()
{
    const float dt = 1.0f / 60.0f;

    Vector3 step = m_Velocity * dt;

    m_Position += step;
    m_Travelled += step.lenght();

    m_EmitTimer -= dt;

    if (m_EmitTimer <= 0.0f)
    {
        m_EmitTimer = TRAIL_INTERVAL;
        EmitTrail();
    }

    // The player first: a shot arriving on the same frame it clips a crate
    // should still be the hit the player felt coming.
    if (HitPlayer() || HitSolid())
    {
        SetDestory();
        return;
    }

    // Out of range. This is the normal end of a wave that hits nothing, and
    // it is measured in distance travelled rather than in time, so the reach
    // does not change if the speed is ever retuned.
    if (m_Travelled >= m_Range)
    {
        SetDestory();
        return;
    }

    m_Life -= dt;

    if (m_Life <= 0.0f)
    {
        SetDestory();
        return;
    }

    GameObject::Update();
}
