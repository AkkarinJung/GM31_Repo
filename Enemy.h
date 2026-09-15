#pragma once

#include "GameObject.h"
#include <DirectXMath.h>

class Enemy : public GameObject
{
private:
    // Decision making - what this enemy wants to do. Movement, animation and
    // damage below stay this class's responsibility.
    class EnemyAI* m_AI = nullptr;

    // What an enemy is worth on the FIRST stage. Kept separate from the live
    // values below so ScaleForStage can be applied to the base every time
    // rather than to whatever the last call left behind - scaling a scaled
    // number is how a stage 5 enemy ends up with thousands of HP.
    const int m_BaseMaxHP = 30;        // a few sword hits to kill
    const int m_BaseAttackDamage = 20;

    int m_AttackDamage = m_BaseAttackDamage;

    // Where a thrown wave leaves this enemy, and what it aims at, both
    // measured up from the feet - every character here stands on its
    // position. The enemy's body is about 1.4 tall at m_BaseScale and the
    // player's is 1.8, so these put the shot at roughly chest to chest
    // instead of skimming the floor.
    const float m_MuzzleHeight = 0.9f;
    const float m_AimHeight = 0.9f;

    // The melee swing's arc. Smaller and shorter-lived than the player's
    // 1.5 x 1.5 over 0.18s: the enemy's swing has to read as an answer to
    // the player's, not as the same event. The arc carries no damage - the
    // reach is CanReachTarget's business, exactly as the player's hitbox is
    // the weapon's - so these are free to be tuned purely for readability.
    const float m_SwingEffectReach = 0.8f;    // how far in front of the enemy
    const float m_SwingEffectSize = 1.1f;
    const float m_SwingEffectLifetime = 0.16f;
    const float m_SwingEffectSweep = 1.1f;

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

    void AttackTarget();

    // The one-shot arc a melee swing leaves behind. Direction is where the
    // swing is aimed; it does not have to be normalised.
    void SpawnSwingEffect(const Vector3& Direction);

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

    // The toon shader and its ramp used to be built here, per enemy. They
    // live in ToonShader now - the scenery wanted the same look, and a copy
    // per object would have meant hundreds of them. See ToonShader::Bind.

    // One of these two, never both. ModelRenderer parses Wavefront OBJ and
    // AnimationModel goes through assimp, so which one an enemy gets is
    // decided by the file it is given - see LoadModel. Everything else talks
    // to whichever exists through SetModelFlash.
    class ModelRenderer* m_ModelRenderer = nullptr;
    class AnimationModel* m_AnimationModel = nullptr;

    // Lifts the drawn mesh so it stands on m_Position like every other
    // character here. Measured from an FBX's own bounds, because a model
    // built around its middle would otherwise sink halfway into the floor.
    float m_ModelOffsetY = 0.0f;

    // Drives the flash on whichever model component this enemy has.
    void SetModelFlash(bool Flash, const XMFLOAT4& Colour);

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

    // The ground under this enemy. Cached, because finding it is a
    // dynamic_cast over every object in the scene and it was being done once
    // per ground enemy per frame - see Manager::GetGameObj.
    class MeshField* m_MeshField = nullptr;

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

    // Applied by the spawner straight after this enemy is built, before
    // anything reads its stats. Both multipliers are against the base
    // values, so calling it twice with the same numbers changes nothing.
    void ScaleForStage(float HPScale, float DamageScale);

    // The mesh this enemy wears. Called by the spawner right after the
    // object is built, before anything draws. An .obj goes through
    // ModelRenderer and anything else through AnimationModel; an FBX is also
    // measured and scaled to match the collision body, so a new model does
    // not have to be authored at any particular size to look right.
    //
    // Only the first call does anything - a GameObject cannot drop a
    // component once it has one, so a second mesh would simply draw on top
    // of the first.
    void LoadModel(const char* FileName);

    // A shot this enemy threw was parried on arrival. The answer is the same
    // one a parried swing gets, and it lives here rather than in EnemyShot so
    // both routes cannot drift apart - the stun length and the flash are this
    // class's business.
    void OnShotParried();

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