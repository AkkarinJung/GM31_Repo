#pragma once

#include "GameObject.h"
#include <DirectXMath.h>

class Enemy : public GameObject
{
private:
    // Decision making - what this enemy wants to do. Movement, animation and
    // damage below stay this class's responsibility.
    class EnemyAI* m_AI = nullptr;
    int m_AttackDamage = 20;

    void AttackTarget();

    // Sine Wave
    float m_Time = 0.0f;
    float m_Amplitude = 0.7f;
    float m_Frequency = 5.0f;

    float m_BaseScale = 0.7f; // overall size multiplier - shrink the enemy a bit; tune to taste

    // Collision / physics
    Vector3 m_Velocity = Vector3(0.0f, 0.0f, 0.0f);
    float m_Radius = 1.0f;
    float m_Mass = 1.0f;

    // Shader
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    // Toon ramp texture, sampled at t1 by toonPS
    ID3D11ShaderResourceView* m_RampTexture = nullptr;

    // x = ramp row, y = edge threshold, z = edge darkening
    XMFLOAT4 m_Parameter{ 0.125f, -0.35f, 0.3f, 0.15f };

    class ModelRenderer* m_ModelRenderer;

    Vector3 m_Shake;
    float m_ShakeTime;

    class Stats* m_Stats = nullptr;
    bool m_Flash;

    GameObject* m_Shadow;

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    void Shake(Vector3 Shake)
    {
        m_Shake = Shake;
        m_ShakeTime = 0;
    }
    void AddDamage(int Damage);

    // Spawn-time configuration: Manager::AddGameObj<Enemy>()->GetAI()->Configure(...)
    class EnemyAI* GetAI() const { return m_AI; }

};