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

    // The swing is announced before it lands. AttackTarget used to fire on
    // the same frame the AI asked for it, so there was nothing to read and
    // nothing to react to - this window is what the player parries.
    bool m_AttackPending = false;
    float m_AttackWindup = 0.0f;     // counts down to the strike
    float m_AttackWindupTime = 0.0f; // what it started at, for the flash ramp
    const float m_AttackWindupRatio = 0.7f; // of the AI's attack duration
    const float m_ParryStunTime = 1.2f;     // a parried enemy is left wide open

    void AttackTarget();

    // Sine Wave
    float m_Time = 0.0f;
    float m_Amplitude = 0.7f;
    float m_Frequency = 5.0f;

    float m_BaseScale = 0.7f; // overall size multiplier - shrink the enemy a bit; tune to taste

    // Collision / physics
    Vector3 m_Velocity = Vector3(0.0f, 0.0f, 0.0f);
    float m_Radius = 1.0f;

    // Collision body, standing on m_Position - the same box the player uses,
    // resolved by the same code.
    Vector3 m_BodyHalfSize{ 0.5f, 0.7f, 0.5f };

    // How close this enemy may get to the player before it steps back. The
    // player is never moved by this - only the enemy is.
    const float m_PlayerSeparation = 1.1f;

    // Push resistance. A separation step moves this enemy by 1 / m_Mass of
    // the overlap, so a heavy enemy shrugs off a shove and takes a few
    // frames to give ground instead of sliding away in one.
    float m_PushGiveMin = 0.1f; // even the heaviest still yields eventually
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

    // Heavier enemies resist being shoved, by the player and by each other.
    void SetMass(float Mass) { m_Mass = Mass; }
    float GetMass() const { return m_Mass; }

    // True while the swing is telegraphing. An enemy committed to an attack
    // plants itself - it cannot be shoved out of its own swing.
    bool IsWindingUp() const { return m_AttackPending; }

};