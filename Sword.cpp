#include "main.h"
#include "Sword.h"
#include "modelRenderer.h"
#include "animationModel.h"
#include "Enemy.h"
#include "manager.h"
#include "Collision.h"

void Sword::LoadModel()
{
    //OBJ
    //m_ModelRenderer = AddGameComponent<ModelRenderer>(this);
    //m_ModelRenderer->Load("asset\\model\\sword.obj"); // TODO: replace with a real sword model

    //FBX
    m_AnimationModel = AddGameComponent<AnimationModel>(this);
    m_AnimationModel->Load("asset\\model\\Akai.fbx"); // TODO: point at a real sword fbx

    m_Damage = 3.0f;
    m_Cooldown = 0.5f;
}

void Sword::Use(GameObject* Owner)
{
    if (!CanUse())
        return;

    m_CooldownTimer = m_Cooldown;

    Vector3 ownerPos = Owner->GetPosition();
    Vector3 forward = Owner->GetFoward();
    forward.y = 0.0f;
    forward.normalize();

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
            continue; // outside the swing's front arc

        enemy->AddDamage((int)m_Damage);
        enemy->Shake(forward * 0.3f);
    }
}