#pragma once
#include <vector>
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
    float m_Range = 2.0f;          // how far the weapon reaches, HORIZONTALLY
    float m_CriticalDamage = 1.5f; // damage multiplier on a critical hit

    // The vertical half of the reach. The test is 2.5D like everything else
    // here: horizontal distance against m_Range, height against this.
    // Measuring one 3D distance instead meant jumping shortened the sword.
    //
    // Deliberately the same number, and the same shape of test, as the
    // enemies' own EnemyAIConfig::AttackHeight - what can hit the player and
    // what the player can hit should be described the same way, or neither
    // side can be tuned without surprising the other.
    float m_VerticalReach = 1.5f;

    // Everything this swing has already damaged. The active window is several
    // frames long now (see the hit window in Player::Update), so without this
    // one swing would damage the same enemy once per frame it stayed in the
    // arc. Only ever compared, never dereferenced.
    std::vector<GameObject*> m_HitThisSwing;

    // The critical roll belongs to the SWING, not to the frame. It was
    // already rolled once per Use() rather than once per enemy so that a
    // swing hitting two enemies crits on both or neither; now that Use() runs
    // on several frames of one swing, keeping that promise means remembering
    // the roll here instead of re-rolling it.
    bool m_SwingRolled = false;
    bool m_SwingCritical = false;

    bool AlreadyHit(GameObject* Target) const
    {
        for (size_t i = 0; i < m_HitThisSwing.size(); i++)
        {
            if (m_HitThisSwing[i] == Target)
                return true;
        }
        return false;
    }

    void MarkHit(GameObject* Target) { m_HitThisSwing.push_back(Target); }

    // ---- where this frame's hits landed ---------------------------------
    //
    // PURELY for the wielder's impact VFX. Nothing in any damage path reads
    // any of this, so it can be moved, offset or dropped entirely without
    // changing what a swing hits - which is the whole point of keeping it
    // here rather than letting the effect work out its own position by
    // re-running the reach test.
    //
    // Refilled every frame of the active window, not once per swing: the
    // wielder spawns a burst on the frame a target is first damaged, and a
    // per-swing list would make it re-spawn one for every earlier target on
    // every later frame.
    //
    // A fixed array rather than a vector because this is rewritten sixty
    // times a second. A swing landing on more targets than this still damages
    // all of them - it just stops recording positions, so the extra ones get
    // no burst.
    static const int FRAME_HIT_MAX = 8;

    Vector3 m_FrameHitPoint[FRAME_HIT_MAX];
    Vector3 m_FrameHitDirection[FRAME_HIT_MAX];
    int m_FrameHitCount = 0;

    void ClearFrameHits() { m_FrameHitCount = 0; }

    void RecordFrameHit(const Vector3& Point, const Vector3& Direction)
    {
        if (m_FrameHitCount >= FRAME_HIT_MAX)
            return;

        m_FrameHitPoint[m_FrameHitCount] = Point;
        m_FrameHitDirection[m_FrameHitCount] = Direction;
        m_FrameHitCount++;
    }

    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    virtual void LoadModel() {}; // derived class loads its own mesh here

   // Scales the whole swing, not just m_Damage. The weapon's own damage is
   // small next to the wielder's Attack, so a percentage reward applied to
   // m_Damage alone rounds away to nothing - this makes "+15% damage" mean
   // 15% of what the swing actually deals.
    float m_DamageMultiplier = 1.0f;

    // Scales THIS SWING only, on top of m_DamageMultiplier. Which step of a
    // combo is landing is a property of the swing, not of the weapon, so it
    // is kept apart from m_DamageMultiplier on purpose: that one carries the
    // run's permanent rewards, and folding a finisher's bonus into it would
    // make the combo compound itself a little more every time it was used.
    float m_SwingMultiplier = 1.0f;

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    // Opens a swing: forgets what the last one hit and starts the cooldown.
    // Called once when the active window opens, not once per frame - Use()
    // no longer touches the cooldown, because a multi frame window would
    // otherwise put the weapon on cooldown against its own second frame.
    virtual void BeginSwing()
    {
        m_HitThisSwing.clear();
        m_SwingRolled = false;
        m_CooldownTimer = m_Cooldown;
        ClearFrameHits();
    }

    // Fire / swing. Returns true when it hit something it had not already hit
    // during THIS swing, which is what the owner needs to know to add
    // hitstop, shake and so on - so the impact fires once per enemy, not once
    // per frame the enemy spends inside the arc.
    virtual bool Use(GameObject* Owner) { return false; };

    float GetDamage() const { return m_Damage; }
    void SetDamage(float Damage) { m_Damage = Damage; }
    float GetCooldown() const { return m_Cooldown; }
    void SetCooldown(float Cooldown) { m_Cooldown = Cooldown; }
    float GetRange() const { return m_Range; }
    void SetRange(float Range) { m_Range = Range; }
    float GetVerticalReach() const { return m_VerticalReach; }
    void SetVerticalReach(float Reach) { m_VerticalReach = Reach; }
    float GetCriticalDamage() const { return m_CriticalDamage; }
    void SetCriticalDamage(float CriticalDamage) { m_CriticalDamage = CriticalDamage; }
    float GetDamageMultiplier() const { return m_DamageMultiplier; }
    void SetDamageMultiplier(float DamageMultiplier) { m_DamageMultiplier = DamageMultiplier; }

    // Set by the wielder just before BeginSwing, so the first frame of the
    // active window already carries it. Deliberately NOT reset by
    // BeginSwing: a wielder that never sets it simply leaves it at 1.
    float GetSwingMultiplier() const { return m_SwingMultiplier; }
    void SetSwingMultiplier(float SwingMultiplier) { m_SwingMultiplier = SwingMultiplier; }
    bool CanUse() const { return m_CooldownTimer <= 0.0f; }

    // What the last Use() landed on, for the wielder to put impact VFX on.
    // Valid only for the frame Use() was called in - read it immediately
    // after a Use() that returned true.
    int GetFrameHitCount() const { return m_FrameHitCount; }

    const Vector3& GetFrameHitPoint(int Index) const
    {
        return m_FrameHitPoint[(Index < 0 || Index >= m_FrameHitCount) ? 0 : Index];
    }

    const Vector3& GetFrameHitDirection(int Index) const
    {
        return m_FrameHitDirection[(Index < 0 || Index >= m_FrameHitCount) ? 0 : Index];
    }
};
