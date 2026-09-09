#include "main.h"
#include "Weapon.h"

void Weapon::Init()
{
    LoadModel();
}

void Weapon::Update()
{
    if (m_CooldownTimer > 0.0f)
    {
        m_CooldownTimer -= 1.0f / 60.0f;
    }

    GameObject::Update();
}
