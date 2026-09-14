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

    // A swing reaches sideways AND up/down. The vertical reach lives on the
    // AI config (per type) so the decision to attack and the swing that
    // follows cannot disagree - they did, and the mismatch burned a whole
    // attack state and cooldown on a swing that was then rejected.
    const float m_AttackSlack = 0.15f;  // grace for the player edging away during the telegraph
    const float m_TargetHeight = 1.8f;  // how tall the thing it swings at is

    void AttackTarget();

    // Is the target inside this enemy's swing right now? Used twice: to
    // decide whether the telegraph is worth starting, and again when the
    // swing lands, since the target may have walked out during the wind-up.
    bool CanReachTarget() const;

    // Sine Wave
    float m_Time = 0.0f;
    float m_Frequency = 5.0f;

    float m_BaseScale = 0.7f; // overall size multiplier - shrink the enemy a bit; tune to taste

    // Collision / physics
    Vector3 m_Velocity = Vector3(0.0f, 0.0f, 0.0f);

    // How much room an enemy wants from another enemy, as a radius - so two
    // of them settle 2 * m_Radius apart.
    //
    // This was 1.0, which asked for 2.0 units of clearance while the AI was
    // told to close to StopDistance 1.1 and only steers apart inside
    // SeparationRadius 1.4. Every chasing enemy therefore lived permanently
    // inside its own separation radius, so the hard push below fired every
    // single frame instead of only on a real overlap. 0.55 matches the body
    // half size and lines up with the numbers the AI already uses.
    float m_Radius = 0.55f;
    const float m_Gravity = 98.0f; // same pull the player gets

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

    // No separation - from another enemy or from the player - may move this
    // enemy further than this in one frame.
    //
    // This is the fix for the teleport. Every push below used to resolve its
    // WHOLE overlap in a single frame: two enemies half a body apart snapped
    // 1.4 units, and a nearly stacked pair 1.85. With the attacker's mass at
    // 1000 the other one absorbed effectively all of it at once, which is
    // exactly the "an enemy behind me and suddenly it is somewhere else"
    // case. 0.1 per frame is 6 units/s - faster than the 3.0 chase speed, so
    // a crowd still untangles quickly, but it can never outrun the eye.
    const float m_MaxSeparationStep = 0.1f;

    // Fraction of the remaining overlap a pair resolves per frame. Below 1
    // the correction is spread over a few frames instead of snapping, and
    // the leftover is picked up next frame - it still converges, it just
    // does it visibly.
    const float m_SeparationRelax = 0.5f;

    // What an enemy mid-swing weighs, so neighbours flow around it instead of
    // jostling it off its target. It used to be 1000, which with the
    // unclamped push above meant the OTHER enemy took 100% of the overlap in
    // one frame. 8 still plants the attacker (it gives ~11% of the ground)
    // without turning it into a battering ram.
    const float m_AttackingMass = 8.0f;

    // Shader
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    // Toon ramp texture, sampled at t1 by toonPS
    ID3D11ShaderResourceView* m_RampTexture = nullptr;

    // x = ramp row, y = edge threshold, z = edge darkening
    XMFLOAT4 m_Parameter{ 0.125f, -0.35f, 0.3f, 0.15f };

    class ModelRenderer* m_ModelRenderer;

    // Hit wobble. This is a DRAW offset and nothing else - m_Position is
    // never written by it.
    //
    // It used to be added straight into m_Position every frame:
    //     m_Position += m_Shake * cosf(m_ShakeTime * 100.0f);
    // cos was sampled at 1.667 radians per frame at 60fps, so it did not
    // oscillate around zero, it aliased. The first frame alone displaced the
    // enemy by the full 0.5 the sword asks for, and a sustained combo walked
    // it 2.45 units away from where it actually was. That is what made an
    // enemy jump when you hit it.
    //
    // Initialised explicitly. Vector3's default constructor is "Vector3() {}"
    // and initialises nothing, so these are only zero today because
    // Manager::AddGameObj spells it "new T()" - the parentheses value
    // initialise the whole object. Dropping them would put garbage straight
    // into the shake.
    Vector3 m_Shake{ 0.0f, 0.0f, 0.0f };
    Vector3 m_ShakeOffset{ 0.0f, 0.0f, 0.0f };   // added at Draw time only
    int m_ShakeFlip = 0;     // alternates the offset, so it reads as a buzz
    const float m_ShakeDecay = 0.75f;
    float m_ShakeTime = 0.0f;

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
        m_ShakeOffset = Shake;
        m_ShakeFlip = 0;
        m_ShakeTime = 0;
    }
    // Critical only changes how the damage number reads - the extra damage
    // is already in Damage by the time it gets here.
    void AddDamage(int Damage, bool Critical = false);

    // Spawn-time configuration: Manager::AddGameObj<Enemy>()->GetAI()->Configure(...)
    class EnemyAI* GetAI() const { return m_AI; }

    // Heavier enemies resist being shoved, by the player and by each other.
    void SetMass(float Mass) { m_Mass = Mass; }
    float GetMass() const { return m_Mass; }

    // What a weapon swings at. The sword used to test against this enemy's
    // POSITION with a radius of zero - a single point on the floor - so a
    // swing that visibly buried itself in the enemy's chest missed whenever
    // its feet were a hair outside the sword's range, and every swing taken
    // in mid-air missed because the height counted against the reach.
    float GetHitRadius() const { return m_Radius; }
    float GetBodyHeight() const { return m_BodyHalfSize.y * 2.0f; }


};