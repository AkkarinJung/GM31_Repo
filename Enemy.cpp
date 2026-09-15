#include "main.h"
#include "renderer.h"
#include "Enemy.h"
#include "modelRenderer.h"
#include "animationModel.h"
#include "manager.h"
#include "Explosion.h"
#include "Camera.h"
#include "Score.h"
#include "Shadow.h"
#include "EnemyAI.h"
#include "MeshField.h"
#include "Stats.h"
#include "DamageNumber.h"
#include "Player.h"
#include "EnemyShot.h"
#include "SlashEffect.h"
#include "Collision.h"
#include "SoundEffect.h"
#include "ToonShader.h"
#include <algorithm>
#define NOMINMAX
#include <cmath>
#include <cstring>
#include <cctype>

// What an enemy wears when the spawner does not say. Also the size every
// model is fitted to: the collision body is 1.4 tall (m_BodyHalfSize.y is
// 0.7 either way of the feet), so a mesh matched to it means what the player
// swings at is what the player can see.
static const char* const DEFAULT_ENEMY_MODEL = "asset\\model\\Rabbit\\rabbit_1.obj";
static const float ENEMY_MODEL_HEIGHT = 1.4f;

// Case insensitive, because ".obj" and ".OBJ" name the same kind of file and
// a model that silently took the wrong loader would just fail to appear.
static bool HasExtension(const char* FileName, const char* Extension)
{
    if (FileName == nullptr)
        return false;

    const char* dot = strrchr(FileName, '.');

    if (dot == nullptr)
        return false;

    while (*dot != '\0' && *Extension != '\0')
    {
        if (tolower((unsigned char)*dot) != tolower((unsigned char)*Extension))
            return false;

        dot++;
        Extension++;
    }

    return *dot == *Extension; // both ended together, or neither did
}


void Enemy::Init()
{
    m_Layer = 1;
    m_Scale = { m_BaseScale, m_BaseScale, m_BaseScale };
    m_Flash = true;

    m_Rotation.y -= XM_PI;

    m_Stats = AddGameComponent<Stats>(this);
    m_Stats->SetMaxHP(m_BaseMaxHP); // the stage scaling is applied by the
                                    // spawner, see Enemy::ScaleForStage

    // Default behaviour; the spawner overrides it per enemy type with
    // GetAI()->Configure(...) - no Enemy subclass needed for a new type.
    m_AI = AddGameComponent<EnemyAI>(this);
    m_AI->Configure(EnemyAIConfig::Patroller());

    // No model here. The spawner chooses it per enemy type and calls
    // LoadModel immediately after building this object, before anything is
    // drawn - Draw falls back to the default if it somehow did not.


    // The toon shader and its ramp are shared - see ToonShader.
    ToonShader::LoadShared();

    m_Shadow = Manager::AddGameObj<Shadow>();
    m_Shadow->SetScale({ 1.5f ,1.5f ,1.5f });
}

void Enemy::Uninit()
{
    m_Shadow->SetDestory();

    GameObject::Uninit();
}

void Enemy::Update()
{
    const float dt = 1.0f / 60.0f;

    // Think first, then act. This runs the components - EnemyAI among them -
    // and it used to sit at the BOTTOM of this function, so everything below
    // was acting on the decision the AI made a frame ago. At a chase speed of
    // 3 units/s that is 5cm of stale aim every frame, and it showed up as the
    // enemy consistently swinging at where the player just was.
    GameObject::Update();

    // Hit wobble, decayed here and applied in Draw(). It is deliberately a
    // one frame flip rather than a cosine: at 60fps a 100 rad/s cosine is
    // sampled every 1.667 radians, which does not read as a vibration and
    // does not average to zero either. Alternating the sign every frame is
    // both, and it costs nothing.
    m_ShakeOffset = m_Shake * ((m_ShakeFlip & 1) ? -1.0f : 1.0f);
    m_ShakeFlip++;
    m_Shake *= m_ShakeDecay;
    if (m_Shake.lenght() < 0.001f)
    {
        m_Shake = Vector3(0.0f, 0.0f, 0.0f);
        m_ShakeOffset = Vector3(0.0f, 0.0f, 0.0f);
    }

    m_ShakeTime += dt;
    if (m_ShakeTime > 0.04f)
    {
        m_Flash = false;
    }


    // -----------------------------
    // Act on the AI's decisions - the AI only decides, moving is done here
    // -----------------------------
    Vector3 moveDirection = m_AI->GetMoveDirection();
    float moveSpeed = m_AI->GetMoveSpeed();

    m_Velocity.x = moveDirection.x * moveSpeed;
    // Fliers steer their own altitude. Ground enemies keep whatever falling
    // speed they have built up - assigning 0 here meant the gravity added
    // below never accumulated, so they sank at a constant crawl.
    if (m_AI->IsFlying())
        m_Velocity.y = moveDirection.y * moveSpeed;
    m_Velocity.z = 0.0f;

    std::vector<AABB> solids = Collision::GatherSolids();

    if (m_AI->IsFlying())
    {
        m_Position += m_Velocity * dt; // fliers pass over crates
    }
    else
    {
        // Ground enemies fall, exactly like the player. Without this nothing
        // ever brought one back down: the crate push-out resolves along
        // whichever axis is shallower and can lift an enemy onto a crate, and
        // a hovering enemy telegraphs its swing but can never land it,
        // because AttackTarget checks vertical reach.
        m_Velocity.y -= m_Gravity * dt;

        // One axis at a time, same as the player.
        Collision::MoveX(m_Position, m_BodyHalfSize, m_Velocity.x * dt, solids);

        bool landed = false;
        if (Collision::MoveY(m_Position, m_BodyHalfSize, m_Velocity.y * dt, solids, landed))
            m_Velocity.y = 0.0f;

        MeshField* meshField = Manager::GetGameObj<MeshField>();
        if (meshField != nullptr)
        {
            float ground = meshField->GetHeight(m_Position);
            if (m_Position.y < ground)
            {
                m_Position.y = ground;
                m_Velocity.y = 0.0f;
            }
        }
    }

    m_Position.z = 0.0f; // the player is locked to this plane, so enemies are too

    // The AI asking for an attack starts the telegraph; the hit lands when the
    // telegraph runs out. The AI checks reach before committing (range and
    // vertical band), so anything it asks for here is worth announcing - the
    // swing can still whiff if the player leaves during the wind-up, which is
    // what AttackTarget re-checks.
    if (m_AI->ConsumeAttack())
    {
        m_AttackWindupTime = m_AI->GetAttackDuration() * m_AttackWindupRatio;
        m_AttackWindup = m_AttackWindupTime;
        m_AttackPending = true;

        SoundEffect::Play(SE::EnemyAttack);
    }

    if (m_AttackPending)
    {
        m_AttackWindup -= dt;

        if (m_AttackWindup <= 0.0f)
        {
            m_AttackPending = false;
            AttackTarget();
        }
    }

    // White flash when hurt. While winding up, the enemy pulses red and the
    // pulse brightens as the strike closes in - that is the tell.
    if (m_Flash)
    {
        SetModelFlash(true, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    else if (m_AttackPending && m_AttackWindupTime > 0.0f)
    {
        float t = 1.0f - (m_AttackWindup / m_AttackWindupTime); // 0 at the start, 1 at the strike
        float pulse = fabsf(sinf(t * XM_PI * 3.0f));
        float alpha = 0.25f + 0.75f * (t * 0.6f + pulse * 0.4f);

        SetModelFlash(true, XMFLOAT4(1.0f, 0.15f, 0.15f, alpha));
    }
    else
    {
        SetModelFlash(false, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    m_Time += dt;

    // Smooth yaw (neutral turn)
    float targetYaw = atan2f(m_AI->GetFacing(), 0.0f) + XM_PI;
    float deltaYaw = targetYaw - m_Rotation.y;
    while (deltaYaw > XM_PI)  deltaYaw -= XM_2PI;
    while (deltaYaw < -XM_PI) deltaYaw += XM_2PI;

    // Fast enough that the body is pointing the right way by the time the AI
    // is willing to swing (EnemyAI::FacingTarget gates on the AI's facing, and
    // at 4 rad/s a half turn took 0.79s - long enough to start a swing while
    // still visibly pointing the other way).
    const float turnSpeed = 10.0f;
    float maxStep = turnSpeed * dt;
    if (deltaYaw > maxStep) deltaYaw = maxStep;
    if (deltaYaw < -maxStep) deltaYaw = -maxStep;
    m_Rotation.y += deltaYaw;

    // Squash & stretch
    float bounce = sinf(m_Time * m_Frequency);
    float scaleY = m_BaseScale * (1.0f + bounce * 0.15f);
    float scaleXZ = m_BaseScale * (1.0f - bounce * 0.08f);
    m_Scale.x = scaleXZ;
    m_Scale.y = scaleY;
    m_Scale.z = scaleXZ;

    // -----------------------------
    // Stable enemy-enemy collision
    // -----------------------------
    auto enemies = Manager::GetGameObjs<Enemy>();
    for (Enemy* other : enemies)
    {
        if (other == this) continue;
        if (this > other) continue; // resolve pair once

        // Horizontal only. A 3D push let two enemies at different heights
        // shove each other up and, with no gravity, that used to be permanent.
        Vector3 delta = m_Position - other->m_Position;
        delta.y = 0.0f;

        float distSq = delta * delta;
        float minDist = m_Radius + other->m_Radius;
        float minDistSq = minDist * minDist;

        if (distSq >= minDistSq) continue;

        float dist = sqrtf(distSq);
        Vector3 n;
        if (dist < 0.00001f)
        {
            // Exactly on top of each other - pick a side. dist stays 0 so the
            // penetration below is the full minimum distance; setting it to
            // minDist (what this did before) made the penetration zero, which
            // is the one case where two enemies could never separate at all.
            n = Vector3(1.0f, 0.0f, 0.0f);
            dist = 0.0f;
        }
        else
        {
            n = delta * (1.0f / dist);
        }

        // Positional correction, weighted by mass - an enemy mid swing is
        // heavy, so its neighbours flow around it instead of jostling it off
        // its target.
        //
        // Only a FRACTION of the overlap is resolved per frame and each side
        // is clamped. Resolving all of it at once is what made enemies jump:
        // a pair half a body apart snapped 1.4 units, a stacked pair 1.85,
        // and with the attacker at mass 1000 the other one ate the whole
        // correction on a single frame. The leftover is picked up next frame,
        // so a crowd still untangles - it just does it where you can see it.
        float penetration = (minDist - dist) * m_SeparationRelax;

        float massA = m_AttackPending ? m_AttackingMass : m_Mass;
        float massB = other->m_AttackPending ? other->m_AttackingMass : other->m_Mass;

        float totalMass = massA + massB;
        if (totalMass < 0.00001f) totalMass = 1.0f;

        float moveA = penetration * (massB / totalMass);
        float moveB = penetration * (massA / totalMass);

        if (moveA > m_MaxSeparationStep) moveA = m_MaxSeparationStep;
        if (moveB > other->m_MaxSeparationStep) moveB = other->m_MaxSeparationStep;

        m_Position += n * moveA;
        other->m_Position -= n * moveB;
    }

    // Step out of the player rather than pushing it. The player owns its own
    // position - input, gravity and solids move it and nothing else - so an
    // enemy crowding it just stops against it, and two enemies either side
    // settle instead of batting the player back and forth.
    Player* player = Manager::GetGameObj<Player>();
    if (player != nullptr)
    {
        Vector3 delta = m_Position - player->GetPosition();
        delta.y = 0.0f;

        float distance = delta.lenght();

        // Winding up an attack plants the enemy: walking into a telegraphing
        // enemy no longer shoves it out of its swing, so the tell cannot be
        // cancelled just by pressing into it.
        if (distance < m_PlayerSeparation && !m_AttackPending)
        {
            if (distance > 0.0001f)
                delta /= distance;
            else
                delta = Vector3(1.0f, 0.0f, 0.0f); // exactly on top - pick a side

            // Give ground in proportion to how light it is. The push repeats
            // every frame the player keeps pressing, so a heavy enemy still
            // ends up clear - it just takes longer.
            float give = (m_Mass > 0.0001f) ? (1.0f / m_Mass) : 1.0f;
            if (give > 1.0f) give = 1.0f;
            if (give < m_PushGiveMin) give = m_PushGiveMin;

            // Clamped like the enemy-enemy push above, and for the same
            // reason: walking into an enemy that is already pinned against a
            // crate produced a large one-frame correction that read as a
            // teleport rather than as being shouldered aside.
            float step = (m_PlayerSeparation - distance) * give;
            if (step > m_MaxSeparationStep) step = m_MaxSeparationStep;

            m_Position += delta * step;
        }
    }

    // The separations above are soft pushes; crates still win.
    if (!m_AI->IsFlying())
        Collision::PushOutOfSolids(m_Position, m_BodyHalfSize, solids);
}

void Enemy::Draw()
{
    // Placed here rather than in Update, which does not run while the game is
    // paused - see the note in Player::Draw. Read off the true position, so
    // the hit wobble applied further down never drags the shadow with it.
    Vector3 shadowPos = m_Position;
    shadowPos.y = 0.01f;
    m_Shadow->SetPosition(shadowPos);

    // Nothing gave this enemy a mesh. Only reachable if a future spawner
    // forgets LoadModel; done here rather than in Update because Update does
    // not run while the reward pick has the game paused, and an invisible
    // enemy through that whole screen would look like a bug.
    if (m_ModelRenderer == nullptr && m_AnimationModel == nullptr)
        LoadModel(DEFAULT_ENEMY_MODEL);

    ToonShader::Bind(ToonShader::CharacterLook);

    // The hit wobble and the model's stand-on-its-feet offset both go on
    // here and come straight back off, so the world matrix carries them and
    // m_Position does not. Everything that reads the enemy's position - the
    // AI, the sword, the separation, the shadow - keeps seeing where the
    // enemy actually is.
    Vector3 truePosition = m_Position;
    m_Position += m_ShakeOffset;
    m_Position.y += m_ModelOffsetY;

    GameObject::Draw();

    m_Position = truePosition;
}

bool Enemy::CanReachTarget() const
{
    GameObject* target = m_AI->GetTarget();
    if (target == nullptr)
        return false;

    Vector3 toTarget = target->GetPosition() - m_Position;
    toTarget.y = 0.0f;

    if (toTarget.lenght() > m_AI->GetAttackRange() + m_AttackSlack)
        return false;

    // Vertical reach. Positions are at the feet, so an enemy hovering above
    // the target still connects with its head, while one on the ground
    // cannot reach a target standing on a crate over it.
    // Height comes off the AI config, so this test and the one the AI used to
    // decide to attack are the same test.
    float targetHeight = m_AI->GetAttackTargetHeight();

    float dy = m_Position.y - target->GetPosition().y;
    float verticalGap = 0.0f;

    if (dy > targetHeight)
        verticalGap = dy - targetHeight;    // above the target's head
    else if (dy < 0.0f)
        verticalGap = -dy;                  // target is above this enemy

    return verticalGap <= m_AI->GetAttackHeight();
}

void Enemy::AttackTarget()
{
    GameObject* target = m_AI->GetTarget();
    if (target == nullptr)
        return;

    // The telegraph is a real window: leaving the enemy's reach during it
    // makes the swing whiff.
    if (!CanReachTarget())
        return;

    // Interrupted. The telegraph and the AI's state machine used to be
    // completely independent: hitting an enemy 0.15s into its 0.525s wind-up
    // put the AI into Stunned, but m_AttackWindup kept counting and the swing
    // landed for full damage 0.37s later - while the enemy was still playing
    // its hurt reaction. You read the tell, you answered it, and you got hit
    // anyway with nothing on screen explaining why. AddDamage clears the
    // pending swing now; this is the belt and braces for a stun from
    // anywhere else (a parry, a future ability).
    EnemyState state = m_AI->GetState();
    if (state == EnemyState::Stunned || state == EnemyState::Dead)
        return;

    // Where the attack comes from and what it is aimed at. Chest to chest,
    // measured up from the feet - every character here stands on its
    // position, so aiming at the target's origin would point at the floor.
    Vector3 from = m_Position;
    from.y += m_MuzzleHeight;

    Vector3 to = target->GetPosition();
    to.y += m_AimHeight;

    Vector3 direction = to - from;
    direction.z = 0.0f; // the play plane is XY

    // ---- ranged: the damage travels -------------------------------------
    //
    // The wind-up says a wave is coming, the flight says where, and both
    // dodging and parrying answer it. The parry travels with the damage:
    // EnemyShot asks for it on arrival and calls OnShotParried() below.
    //
    // The wave dies at the end of this enemy's own attack range, so a shot
    // is exactly as long as the reach the AI was allowed to fire from - it
    // cannot sail on across the map and catch someone the enemy never had
    // any business threatening.
    if (m_AI->IsRangedAttack())
    {
        EnemyShot* shot = Manager::AddGameObj<EnemyShot>();
        shot->Fire(this, from, direction, m_AttackDamage, m_AI->GetAttackRange());
        return;
    }

    // ---- melee: the swing lands here, and is a one-shot arc --------------
    //
    // The same sprite and the same shape of call as the player's own swing
    // (see Player::SpawnSlash): one SlashEffect, played once, which sweeps
    // and burns out on its own. It carries no damage - exactly like the
    // player's, where the hitbox is the weapon's business - so it can be
    // sized for readability without touching what the swing actually hits.
    SpawnSwingEffect(direction);

    // Parried: no damage, and the enemy is left open for far longer than a
    // normal hit stun.
    Player* player = dynamic_cast<Player*>(target);
    if (player != nullptr && player->TryParry(this))
    {
        OnShotParried();
        return;
    }

    Stats* stats = target->GetGameComponent<Stats>();
    if (stats != nullptr)
    {
        stats->TakeDamage(m_AttackDamage);

        if (player != nullptr)
            SoundEffect::Play(SE::PlayerHurt);
    }
}

void Enemy::SpawnSwingEffect(const Vector3& Direction)
{
    Vector3 direction = Direction;
    direction.z = 0.0f;

    float length = direction.lenght();

    if (length < 0.0001f)
        direction = Vector3(m_AI->GetFacing() >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
    else
        direction /= length;

    // In front of the enemy rather than inside it, so the arc reads as a
    // swing rather than as a flash on its chest.
    Vector3 position = m_Position;
    position.y += m_MuzzleHeight;
    position += direction * m_SwingEffectReach;

    // The crescent is drawn already bulging along +X, so pointing it down
    // the swing's direction is just its angle. The player needs a
    // hand-authored roll table because a three step combo has three
    // different arcs; this has one.
    float roll = atan2f(direction.y, direction.x);

    // The sweep flips with the facing, so the blade always travels the way
    // the enemy is swinging rather than back into itself.
    float facing = (direction.x < 0.0f) ? -1.0f : 1.0f;

    SlashEffect* slash = Manager::AddGameObj<SlashEffect>();

    // The old crescent, not the player's sword sheet. An enemy's swipe
    // reading exactly like the player's own sword makes a crowded fight hard
    // to follow - whose swing was that is a question the player should never
    // have to ask.
    slash->SetStyle(SlashStyle::Crescent);

    slash->Play(position, Vector3(0.0f, 0.0f, roll),
        Vector3(m_SwingEffectSize, m_SwingEffectSize, 1.0f),
        m_SwingEffectLifetime, 1.0f, facing * m_SwingEffectSweep);
}

void Enemy::LoadModel(const char* FileName)
{
    if (FileName == nullptr)
        return;

    // First call wins. A GameObject cannot drop a component once it has one,
    // so a second mesh would draw on top of the first rather than replace it.
    if (m_ModelRenderer != nullptr || m_AnimationModel != nullptr)
        return;

    // ModelRenderer only parses Wavefront OBJ; anything else goes through
    // assimp, which is what AnimationModel wraps. The same split Box, Sword,
    // Hedge and Prop all make - it is the file that decides, not the caller.
    if (HasExtension(FileName, ".obj"))
    {
        m_ModelRenderer = AddGameComponent<ModelRenderer>(this);
        m_ModelRenderer->Load(FileName);
        return;
    }

    m_AnimationModel = AddGameComponent<AnimationModel>(this);
    m_AnimationModel->Load(FileName);

    // Measure what actually loaded and fit it to the collision body, rather
    // than trusting a new model to arrive at the right scale. m_BaseScale is
    // what the squash and stretch in Update works off, so setting it here
    // keeps that animation proportional to whatever size the mesh needed.
    XMFLOAT3 boundsMin = m_AnimationModel->GetBoundsMin();
    XMFLOAT3 boundsMax = m_AnimationModel->GetBoundsMax();

    float height = boundsMax.y - boundsMin.y;

    if (height > 0.0001f)
        m_BaseScale = ENEMY_MODEL_HEIGHT / height;

    // Stand it on its feet. Positions here are at the feet, so a mesh built
    // around its own middle would sink half of itself into the floor.
    m_ModelOffsetY = -boundsMin.y * m_BaseScale;

    m_Scale = { m_BaseScale, m_BaseScale, m_BaseScale };
}

void Enemy::SetModelFlash(bool Flash, const XMFLOAT4& Colour)
{
    if (m_ModelRenderer != nullptr)
    {
        m_ModelRenderer->SetFlashColor(Colour);
        m_ModelRenderer->SetFlash(Flash);
    }

    if (m_AnimationModel != nullptr)
    {
        m_AnimationModel->SetFlashColor(Colour);
        m_AnimationModel->SetFlash(Flash);
    }
}

void Enemy::ScaleForStage(float HPScale, float DamageScale)
{
    if (m_Stats != nullptr)
    {
        int maxHP = (int)(m_BaseMaxHP * HPScale + 0.5f);
        if (maxHP < 1)
            maxHP = 1;

        // SetMaxHP moves current HP by the same delta, so raising the ceiling
        // on a freshly built enemy leaves it full - which is what a spawn
        // wants. See Stats::SetMaxHP.
        m_Stats->SetMaxHP(maxHP);
    }

    int damage = (int)(m_BaseAttackDamage * DamageScale + 0.5f);
    if (damage < 1)
        damage = 1;

    m_AttackDamage = damage;
}

void Enemy::OnShotParried()
{
    m_AI->Stun(m_ParryStunTime);
    m_Flash = true;
    m_ShakeTime = 0.0f;
}

// The popup is tinted through Material.Diffuse, which multiplies the digit
// sprite rather than replacing it - so this works whatever colour the sheet is.
static const XMFLOAT4 DAMAGE_COLOUR = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
static const XMFLOAT4 CRITICAL_COLOUR = XMFLOAT4(1.0f, 0.22f, 0.18f, 1.0f);

void Enemy::AddDamage(int Damage, bool Critical)
{
    m_Stats->TakeDamage(Damage);
    m_Flash = true;

    // Taking a hit interrupts a swing that has not landed yet. OnDamaged()
    // below only tells the AI, and the AI does not own the wind-up - so
    // without this line the enemy flinched and then hit you anyway. Whether
    // the flinch actually happens is the AI's call (it refuses to be stun
    // locked, see EnemyAI::OnDamaged), but the swing is always cancelled:
    // trading a hit for a hit you already interrupted is the single most
    // unfair thing in the fight.
    m_AttackPending = false;
    m_AttackWindup = 0.0f;

    m_AI->OnDamaged();

    // The hurt grunt only for a hit it survives. On a killing blow the death
    // sound says the same thing better, and the swing already played its own
    // impact - three sounds on one frame just turns to mush.
    if (!m_Stats->IsDead())
        SoundEffect::Play(SE::EnemyHurt);

    DamageNumber* damageNumber = Manager::AddGameObj<DamageNumber>();
    Vector3 headPos = m_Position;
    headPos.y += 2.3f * m_BaseScale; // above the head, scales with the enemy's size
    damageNumber->Init(headPos, Damage, false,
        Critical ? CRITICAL_COLOUR : DAMAGE_COLOUR);

    if (m_Stats->IsDead())
    {
        m_AI->OnDeath();

        // Played from here, not from anything attached to the enemy:
        // SetDestory() below tears the object down and a sound it owned
        // would be cut off in the same frame.
        SoundEffect::Play(SE::EnemyDeath);

        SetDestory();
        Explosion* explosion = Manager::AddGameObj<Explosion>();
        explosion->SetPosition(m_Position);

        // There used to be a "this->SetScale(m_Scale * 2.0f)" here. It scaled
        // the ENEMY, not the explosion, and SetDestory() above means this
        // object is deleted at the end of this same Update - before anything
        // draws - so it never had any effect. The burst's size lives in
        // Explosion::Init.

        Camera* camera = Manager::GetGameObj<Camera>();
        camera->Shake({ 1.0f,1.0f,0.0f });

        Manager::GetGameObj<Score>()->Add(1);
    }
}