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

    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    virtual void LoadModel() {}; // derived class loads its own mesh here

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    virtual void Use(GameObject* Owner) {}; // fire / swing, override per weapon

    float GetDamage() const { return m_Damage; }
    void SetDamage(float Damage) { m_Damage = Damage; }
    void SetCooldown(float Cooldown) { m_Cooldown = Cooldown; }
    bool CanUse() const { return m_CooldownTimer <= 0.0f; }
};
