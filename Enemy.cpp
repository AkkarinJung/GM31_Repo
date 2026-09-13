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
#include "Box.h"
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

    Vector3 oldPosition = m_Position;

    // -----------------------------
    // Act on the AI's decisions - the AI only decides, moving is done here
    // -----------------------------
    Vector3 moveDirection = m_AI->GetMoveDirection();
    float moveSpeed = m_AI->GetMoveSpeed();

    m_Velocity.x = moveDirection.x * moveSpeed;
    m_Velocity.y = m_AI->IsFlying() ? moveDirection.y * moveSpeed : 0.0f;
    m_Velocity.z = 0.0f;

    m_Position += m_Velocity * dt;

    if (!m_AI->IsFlying())
    {
        MeshField* meshField = Manager::GetGameObj<MeshField>();
        if (meshField != nullptr)
            m_Position.y = meshField->GetHeight(m_Position);
    }

    m_Position.z = 0.0f; // the player is locked to this plane, so enemies are too

    // Crates are solid for walking enemies too - they used to stroll
    // straight through. Fliers pass over them.
    if (!m_AI->IsFlying())
    {
        auto boxes = Manager::GetGameObjs<Box>();
        for (auto box : boxes)
        {
            Vector3 boxPosition = box->GetPosition();
            Vector3 boxScale = box->GetScale();

            if (boxPosition.x - boxScale.x < m_Position.x &&
                m_Position.x < boxPosition.x + boxScale.x &&
                boxPosition.z - boxScale.z < m_Position.z &&
                m_Position.z < boxPosition.z + boxScale.z)
            {
                if (boxPosition.y + boxScale.y < m_Position.y &&
                    m_Position.y < boxPosition.y + boxScale.y * 2.0f)
                {
                    // standing on the crate
                    m_Position.y = boxPosition.y + boxScale.y * 2.0f;
                }
                else if (boxPosition.y - boxScale.y < m_Position.y &&
                    m_Position.y < boxPosition.y + boxScale.y)
                {
                    // walked into the side - stay where it was
                    m_Position.x = oldPosition.x;
                }
            }
        }
    }

    // The AI asking for an attack starts the telegraph; the hit lands when
    // the telegraph runs out.
    if (m_AI->ConsumeAttack())
    {
        m_AttackWindupTime = m_AI->GetAttackDuration() * m_AttackWindupRatio;
        m_AttackWindup = m_AttackWindupTime;
        m_AttackPending = true;
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

        Vector3 delta = m_Position - other->m_Position;
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

        // positional correction
        float penetration = minDist - dist;
        float totalMass = m_Mass + other->m_Mass;
        if (totalMass < 0.00001f) totalMass = 1.0f;

        float moveA = penetration * (other->m_Mass / totalMass);
        float moveB = penetration * (m_Mass / totalMass);

        m_Position += n * moveA;
        other->m_Position -= n * moveB;
    }

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

void Enemy::AttackTarget()
{
    GameObject* target = m_AI->GetTarget();
    if (target == nullptr)
        return;

    // The telegraph is a real window: leaving the enemy's reach during it
    // makes the swing whiff.
    Vector3 toTarget = target->GetPosition() - m_Position;
    toTarget.y = 0.0f;

    if (toTarget.lenght() > m_AI->GetAttackRange() + 0.4f)
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
        stats->TakeDamage(m_AttackDamage);
}

void Enemy::AddDamage(int Damage)
{
    m_Stats->TakeDamage(Damage);
    m_Flash = true;
    m_AI->OnDamaged();

    DamageNumber* damageNumber = Manager::AddGameObj<DamageNumber>();
    Vector3 headPos = m_Position;
    headPos.y += 1.5f * m_BaseScale; // above the head, scales with the enemy's size
    damageNumber->Init(headPos, Damage);

    if (m_Stats->IsDead())
    {
        m_AI->OnDeath();

        SetDestory();
        Explosion* explosion = Manager::AddGameObj<Explosion>();
        explosion->SetPosition(m_Position);
        this->SetScale(m_Scale * 2.0f);

        Camera* camera = Manager::GetGameObj<Camera>();
        camera->Shake({ 1.0f,1.0f,0.0f });

        Manager::GetGameObj<Score>()->Add(1);
    }
}