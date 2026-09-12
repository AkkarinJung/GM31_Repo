#pragma once
#include "component.h"

// Generic combat stats - HP, attack, defense. Attach to any GameObject
// (Player, Enemy, ...) that can deal or take damage. Weapons read the
// wielder's Attack and apply damage to the target's Stats, so no weapon
// needs to know what kind of GameObject it's hitting.
class Stats : public Component
{
private:
    int m_MaxHP = 100;
    int m_HP = 100;
    int m_MaxMP = 50;
    int m_MP = 50;
    int m_Attack = 10;
    int m_Defense = 0;
    float m_CriticalChance = 0.0f; // 0.0f = never crits; rewards raise it

public:
    using Component::Component;

    int GetHP() const { return m_HP; }
    int GetMaxHP() const { return m_MaxHP; }
    int GetMP() const { return m_MP; }
    int GetMaxMP() const { return m_MaxMP; }
    int GetAttack() const { return m_Attack; }
    int GetDefense() const { return m_Defense; }
    float GetCriticalChance() const { return m_CriticalChance; }

    void SetMaxHP(int MaxHP) { m_MaxHP = MaxHP; m_HP = MaxHP; }
    void SetMaxMP(int MaxMP) { m_MaxMP = MaxMP; m_MP = MaxMP; }

    void SetAttack(int Attack) { m_Attack = Attack; }
    void SetDefense(int Defense) { m_Defense = Defense; }
    void SetCriticalChance(float CriticalChance) { m_CriticalChance = CriticalChance; }

    bool IsDead() const { return m_HP <= 0; }

    void TakeDamage(int Amount)
    {
        int reduced = Amount - m_Defense;
        if (reduced < 0)
            reduced = 0;

        m_HP -= reduced;
        if (m_HP < 0)
            m_HP = 0;
    }

    void Heal(int Amount)
    {
        m_HP += Amount;
        if (m_HP > m_MaxHP)
            m_HP = m_MaxHP;
    }

    bool TrySpendMP(int Amount)
    {
        if (m_MP < Amount)
            return false;

        m_MP -= Amount;
        return true;
    }

    void RestoreMP(int Amount)
    {
        m_MP += Amount;
        if (m_MP > m_MaxMP)
            m_MP = m_MaxMP;
    }
};