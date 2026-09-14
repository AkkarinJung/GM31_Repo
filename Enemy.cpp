#include "main.h"
#include "renderer.h"
#include "Enemy.h"
#include "modelRenderer.h"
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
#include "Collision.h"
#include "SoundEffect.h"
#include <algorithm>
#define NOMINMAX
#include <cmath>

void Enemy::Init()
{
    m_Layer = 1;
    m_Scale = { m_BaseScale, m_BaseScale, m_BaseScale };
    m_Flash = true;

    m_Rotation.y -= XM_PI;

    m_Stats = AddGameComponent<Stats>(this);
    m_Stats->SetMaxHP(30); // a few sword hits to kill - tune as needed

    // Default behaviour; the spawner overrides it per enemy type with
    // GetAI()->Configure(...) - no Enemy subclass needed for a new type.
    m_AI = AddGameComponent<EnemyAI>(this);
    m_AI->Configure(EnemyAIConfig::Patroller());

    m_ModelRenderer = AddGameComponent<ModelRenderer>(this);
    m_ModelRenderer->Load("asset\\model\\Rabbit\\rabbit_1.obj");


    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\toonVS.cso");
    Renderer::CreatePixelShader(&m_PixelShader, "shader\\toonPS.cso");

    // トゥーンランプテクスチャ読込
    TexMetadata metadata;
    ScratchImage image;
    LoadFromWICFile(L"asset\\texture\\toon_ramp.png", WIC_FLAGS_NONE, &metadata, image);
    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &m_RampTexture);
    assert(m_RampTexture);

    m_Shadow = Manager::AddGameObj<Shadow>();
    m_Shadow->SetScale({ 1.5f ,1.5f ,1.5f });
}

void Enemy::Uninit()
{
    m_Shadow->SetDestory();

    if (m_VertexLayout) { m_VertexLayout->Release(); m_VertexLayout = nullptr; }
    if (m_VertexShader) { m_VertexShader->Release(); m_VertexShader = nullptr; }
    if (m_PixelShader) { m_PixelShader->Release();  m_PixelShader = nullptr; }
    if (m_RampTexture) { m_RampTexture->Release(); m_RampTexture = nullptr; }

    GameObject::Uninit();
}

void Enemy::Update()
{
    const float dt = 1.0f / 60.0f;

    m_Position += m_Shake * cosf(m_ShakeTime * 100.0f);
    m_ShakeTime += dt;
    m_Shake *= 0.9;
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
        m_ModelRenderer->SetFlashColor(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
        m_ModelRenderer->SetFlash(true);
    }
    else if (m_AttackPending && m_AttackWindupTime > 0.0f)
    {
        float t = 1.0f - (m_AttackWindup / m_AttackWindupTime); // 0 at the start, 1 at the strike
        float pulse = fabsf(sinf(t * XM_PI * 3.0f));
        float alpha = 0.25f + 0.75f * (t * 0.6f + pulse * 0.4f);

        m_ModelRenderer->SetFlashColor(XMFLOAT4(1.0f, 0.15f, 0.15f, alpha));
        m_ModelRenderer->SetFlash(true);
    }
    else
    {
        m_ModelRenderer->SetFlash(false);
    }

    m_Time += dt;

    // Smooth yaw (neutral turn)
    float targetYaw = atan2f(m_AI->GetFacing(), 0.0f) + XM_PI;
    float deltaYaw = targetYaw - m_Rotation.y;
    while (deltaYaw > XM_PI)  deltaYaw -= XM_2PI;
    while (deltaYaw < -XM_PI) deltaYaw += XM_2PI;

    const float turnSpeed = 4.0f;
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
            n = Vector3(1.0f, 0.0f, 0.0f);
            dist = minDist;
        }
        else
        {
            n = delta * (1.0f / dist);
        }

        // positional correction, weighted by mass - and an enemy mid swing
        // counts as immovable, so its neighbours flow around it instead of
        // jostling it off its target.
        float penetration = minDist - dist;

        float massA = m_AttackPending ? 1000.0f : m_Mass;
        float massB = other->m_AttackPending ? 1000.0f : other->m_Mass;

        float totalMass = massA + massB;
        if (totalMass < 0.00001f) totalMass = 1.0f;

        float moveA = penetration * (massB / totalMass);
        float moveB = penetration * (massA / totalMass);

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

            m_Position += delta * ((m_PlayerSeparation - distance) * give);
        }
    }

    // The separations above are soft pushes; crates still win.
    if (!m_AI->IsFlying())
        Collision::PushOutOfSolids(m_Position, m_BodyHalfSize, solids);

    Vector3 shadowPos = m_Position;
    shadowPos.y = 0.01f;
    m_Shadow->SetPosition(shadowPos);

    GameObject::Update();
}

void Enemy::Draw()
{
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    Renderer::SetParameter(m_Parameter);

    // t0 is the model's own texture, set by ModelRenderer::Draw.
    // Only the ramp has to be bound here.
    Renderer::GetDeviceContext()->PSSetShaderResources(1, 1, &m_RampTexture);

    GameObject::Draw();
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
    float dy = m_Position.y - target->GetPosition().y;
    float verticalGap = 0.0f;

    if (dy > m_TargetHeight)
        verticalGap = dy - m_TargetHeight;  // above the target's head
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

    // Parried: no damage, and the enemy is left open for far longer than a
    // normal hit stun.
    Player* player = dynamic_cast<Player*>(target);
    if (player != nullptr && player->TryParry(this))
    {
        m_AI->Stun(m_ParryStunTime);
        m_Flash = true;
        m_ShakeTime = 0.0f;
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

void Enemy::AddDamage(int Damage)
{
    m_Stats->TakeDamage(Damage);
    m_Flash = true;
    m_AI->OnDamaged();

    // The hurt grunt only for a hit it survives. On a killing blow the death
    // sound says the same thing better, and the swing already played its own
    // impact - three sounds on one frame just turns to mush.
    if (!m_Stats->IsDead())
        SoundEffect::Play(SE::EnemyHurt);

    DamageNumber* damageNumber = Manager::AddGameObj<DamageNumber>();
    Vector3 headPos = m_Position;
    headPos.y += 1.5f * m_BaseScale; // above the head, scales with the enemy's size
    damageNumber->Init(headPos, Damage);

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
        this->SetScale(m_Scale * 2.0f);

        Camera* camera = Manager::GetGameObj<Camera>();
        camera->Shake({ 1.0f,1.0f,0.0f });

        Manager::GetGameObj<Score>()->Add(1);
    }
}