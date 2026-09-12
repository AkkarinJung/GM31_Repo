#include "main.h"
#include "Sword.h"
#include "modelRenderer.h"
#include "animationModel.h"
#include "Enemy.h"
#include "manager.h"
#include "Collision.h"
#include "Stats.h"

void Sword::LoadModel()
{
    //OBJ
    //m_ModelRenderer = AddGameComponent<ModelRenderer>(this);
    //m_ModelRenderer->Load("asset\\model\\sword.obj"); // TODO: replace with a real sword model

    //FBX
    m_AnimationModel = AddGameComponent<AnimationModel>(this);
    m_AnimationModel->Load("asset\\model\\sword.fbx"); // TODO: point at a real sword fbx

    m_Damage = 3.0f;
    m_Cooldown = 0.15f; // just a safety rail - the swing animation paces the
                        // combo now. A cooldown longer than the animation let
                        // a swing play with no damage behind it.
}

bool Sword::Use(GameObject* Owner)
{
    if (!CanUse())
        return false;

    m_CooldownTimer = m_Cooldown;

    Vector3 ownerPos = Owner->GetPosition();
    Vector3 forward = Owner->GetFoward();
    forward.y = 0.0f;
    forward.normalize();

    Stats* ownerStats = Owner->GetGameComponent<Stats>();

    // All of this stays in float until the very end. Rounding to int at each
    // step threw away every percentage reward: +15% of 3.0 damage is 0.45,
    // which (int) truncated straight back to the original number.
    float damage = m_Damage + (ownerStats != nullptr ? (float)ownerStats->GetAttack() : 0.0f);
    damage *= m_DamageMultiplier;

    float criticalChance = ownerStats != nullptr ? ownerStats->GetCriticalChance() : 0.0f;
    if ((float)rand() / RAND_MAX < criticalChance)
        damage *= m_CriticalDamage;

    int attackPower = (int)(damage + 0.5f); // round, don't truncate
    if (attackPower < 1)
        attackPower = 1;

    bool hit = false;

    auto enemies = Manager::GetGameObjs<Enemy>();
    for (auto enemy : enemies)
    {
        if (!Collision::SphereVsSphere(ownerPos, m_Range, enemy->GetPosition(), 0.0f))
            continue;

        Vector3 toEnemy = enemy->GetPosition() - ownerPos;
        toEnemy.y = 0.0f;
        float length = toEnemy.lenght();

        if (length <= 0.0f)
            continue;

        toEnemy /= length;

        if (Vector3::dot(forward, toEnemy) < m_AngleDot)
            continue;

        enemy->AddDamage(attackPower);
        enemy->Shake(forward * 0.5f);
        hit = true;
    }

    return hit;
}