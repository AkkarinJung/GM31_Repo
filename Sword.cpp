#include "main.h"
#include "Sword.h"
#include "modelRenderer.h"
#include "animationModel.h"
#include "Enemy.h"
#include "Crate.h"
#include "manager.h"
#include "Collision.h"
#include "Stats.h"

void Sword::LoadModel()
{
    //OBJ
    //m_ModelRenderer = AddGameComponent<ModelRenderer>(this);
    //m_ModelRenderer->Load("asset\\model\\sword.obj"); // TODO: replace with a real sword model

    //FBX
    m_AnimationModel = AddGameComponent<AnimationModel>(this);
    m_AnimationModel->Load("asset\\model\\sword.fbx"); // TODO: point at a real sword fbx

    m_Damage = 3.0f;
    m_Cooldown = 0.15f; // just a safety rail - the swing animation paces the
                        // combo now. A cooldown longer than the animation let
                        // a swing play with no damage behind it.
}

bool Sword::Use(GameObject* Owner)
{
    // No CanUse()/cooldown here any more - BeginSwing() owns both. This is
    // called on every frame of the swing's active window, and a cooldown set
    // on the first of those frames would reject all the rest.
    Vector3 ownerPos = Owner->GetPosition();
    Vector3 forward = Owner->GetFoward();
    forward.y = 0.0f;
    forward.normalize();

    Stats* ownerStats = Owner->GetGameComponent<Stats>();

    // All of this stays in float until the very end. Rounding to int at each
    // step threw away every percentage reward: +15% of 3.0 damage is 0.45,
    // which (int) truncated straight back to the original number.
    float damage = m_Damage + (ownerStats != nullptr ? (float)ownerStats->GetAttack() : 0.0f);

    // Two separate multipliers, and they must stay separate: m_DamageMultiplier
    // is the run's permanent rewards, m_SwingMultiplier is which step of the
    // combo this particular swing is (see Player::SwingDamageScale).
    damage *= m_DamageMultiplier * m_SwingMultiplier;

    // Rolled once for the swing, not once per enemy and not once per frame of
    // the active window, so a swing that hits two enemies crits on both or
    // neither - one roll, one number colour. BeginSwing() clears the flag.
    if (!m_SwingRolled)
    {
        float criticalChance = ownerStats != nullptr ? ownerStats->GetCriticalChance() : 0.0f;
        m_SwingCritical = ((float)rand() / RAND_MAX < criticalChance);
        m_SwingRolled = true;
    }

    bool critical = m_SwingCritical;
    if (critical)
        damage *= m_CriticalDamage;

    int attackPower = (int)(damage + 0.5f); // round, don't truncate
    if (attackPower < 1)
        attackPower = 1;

    bool hit = false;

    auto enemies = Manager::GetGameObjs<Enemy>();
    for (auto enemy : enemies)
    {
        // One hit per enemy per swing - the window is several frames long.
        if (AlreadyHit(enemy))
            continue;

        // 2.5D reach, matching how the enemy measures its own swing at the
        // player (Enemy::CanReachTarget). The old test was
        //     SphereVsSphere(ownerPos, m_Range, enemy->GetPosition(), 0.0f)
        // which is one 3D distance from the player's FEET to the enemy's
        // FEET, with the enemy given a radius of zero - a point on the floor.
        // Two things fell out of that, and both of them read as the hit
        // detection being broken:
        //
        //   * the enemy's body counted for nothing, so a swing that visibly
        //     landed in its chest missed if its feet were 2.1 units away;
        //
        //   * height counted against the reach, so every swing taken in the
        //     air missed. An enemy 1.5 across and 1.5 below is 2.12 away in
        //     3D and was rejected, even though it is well inside the arc.
        Vector3 toEnemy = enemy->GetPosition() - ownerPos;
        toEnemy.y = 0.0f;

        float length = toEnemy.lenght();

        // Horizontal reach, against the enemy's BODY rather than a point.
        if (length > m_Range + enemy->GetHitRadius())
            continue;

        // Vertical reach, written the same way round as Enemy::CanReachTarget
        // so the two are directly comparable. Positions are at the feet, so
        // swinging down at something shorter than you costs nothing until you
        // clear its head, while something above you is measured from your own
        // feet.
        float dy = ownerPos.y - enemy->GetPosition().y; // + = owner is above
        float verticalGap = 0.0f;

        if (dy > enemy->GetBodyHeight())
            verticalGap = dy - enemy->GetBodyHeight();  // above the enemy's head
        else if (dy < 0.0f)
            verticalGap = -dy;                          // enemy is above the owner

        if (verticalGap > m_VerticalReach)
            continue;

        if (length <= 0.0f)
            continue;

        toEnemy /= length;

        if (Vector3::dot(forward, toEnemy) < m_AngleDot)
            continue;

        MarkHit(enemy);

        enemy->AddDamage(attackPower, critical);
        enemy->Shake(forward * 0.5f);
        hit = true;
    }

    // Breakable crates, measured exactly the same way. A crate answers
    // GetHitRadius and GetBodyHeight the way an enemy does precisely so this
    // can be the same test rather than a second, subtly different one - a
    // swing that visibly lands on a crate has to break it, or the crate
    // reads as scenery and the player stops trying.
    auto crates = Manager::GetGameObjs<Crate>();
    for (auto crate : crates)
    {
        if (AlreadyHit(crate))
            continue;

        Vector3 toCrate = crate->GetPosition() - ownerPos;
        toCrate.y = 0.0f;

        float length = toCrate.lenght();

        if (length > m_Range + crate->GetHitRadius())
            continue;

        float dy = ownerPos.y - crate->GetPosition().y; // + = owner is above
        float verticalGap = 0.0f;

        if (dy > crate->GetBodyHeight())
            verticalGap = dy - crate->GetBodyHeight();
        else if (dy < 0.0f)
            verticalGap = -dy;

        if (verticalGap > m_VerticalReach)
            continue;

        if (length <= 0.0f)
            continue;

        toCrate /= length;

        if (Vector3::dot(forward, toCrate) < m_AngleDot)
            continue;

        MarkHit(crate);

        crate->AddDamage(attackPower, critical);
        crate->Shake(forward * 0.35f);
        hit = true;
    }

    return hit;
}