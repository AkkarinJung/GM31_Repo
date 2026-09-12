#pragma once
#include "renderer.h"
#include "GameObject.h"

// Base class for every weapon. Concrete weapons only override LoadModel()
// and Use(); AttachPoint handles following the owner, so no subclass has
// to reimplement that.
class Weapon : public GameObject
{
protected:

    float m_Damage = 1.0f;
    float m_Cooldown = 0.3f;
    float m_CooldownTimer = 0.0f;
    float m_Range = 2.0f;          // how far the weapon reaches
    float m_CriticalDamage = 1.5f; // damage multiplier on a critical hit

    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    virtual void LoadModel() {}; // derived class loads its own mesh here

   // Scales the whole swing, not just m_Damage. The weapon's own damage is
   // small next to the wielder's Attack, so a percentage reward applied to
   // m_Damage alone rounds away to nothing - this makes "+15% damage" mean
   // 15% of what the swing actually deals.
    float m_DamageMultiplier = 1.0f;

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    // Fire / swing. Returns true when it actually hit something, which is
    // what the owner needs to know to add hitstop, shake and so on.
    virtual bool Use(GameObject* Owner) { return false; };

    float GetDamage() const { return m_Damage; }
    void SetDamage(float Damage) { m_Damage = Damage; }
    float GetCooldown() const { return m_Cooldown; }
    void SetCooldown(float Cooldown) { m_Cooldown = Cooldown; }
    float GetRange() const { return m_Range; }
    void SetRange(float Range) { m_Range = Range; }
    float GetCriticalDamage() const { return m_CriticalDamage; }
    void SetCriticalDamage(float CriticalDamage) { m_CriticalDamage = CriticalDamage; }
    float GetDamageMultiplier() const { return m_DamageMultiplier; }
    void SetDamageMultiplier(float DamageMultiplier) { m_DamageMultiplier = DamageMultiplier; }
    bool CanUse() const { return m_CooldownTimer <= 0.0f; }
};
