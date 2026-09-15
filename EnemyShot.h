#pragma once
#include "GameObject.h"

// A slash wave thrown by an enemy.
//
// Every enemy attack is one of these now - the swing that used to apply
// damage the instant the telegraph ended (see Enemy::AttackTarget) throws
// this instead, so the damage has to travel and the player has a flight to
// answer rather than a single frame.
//
// It reaches only as far as the enemy that threw it could attack from -
// see m_Range - so a wave is a threat inside that enemy's zone and nothing
// outside it.
//
// It draws NOTHING itself. The picture is the player's own attack sprite:
// it re-emits a short SlashEffect along its path, which is the same crescent,
// the same shared texture and the same shaders the sword uses. That is why
// this object has an empty Draw - the trail it leaves behind IS the
// projectile, and SlashEffect needed no changes to provide it.
//
// Parrying works the way it does against a swing: the shot asks
// Player::TryParry when it arrives, and a successful parry destroys it and
// leaves the enemy that fired it wide open.
class EnemyShot : public GameObject
{
private:
    Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
    float m_Life = 0.0f;
    int m_Damage = 0;

    // How far this wave may travel, and how far it has. The cap is the
    // firing range of the enemy that threw it, so a shot reaches exactly as
    // far as that enemy was allowed to attack from and no further - it
    // cannot sail on across the map and hit someone it never threatened.
    float m_Range = 0.0f;
    float m_Travelled = 0.0f;

    // Which way the crescent points. Worked out once from the launch
    // direction - the shot flies straight, so it can never need updating.
    float m_Roll = 0.0f;

    float m_EmitTimer = 0.0f;

    // The enemy that fired it, so a parry can answer the right one.
    //
    // ONLY EVER COMPARED, never dereferenced without checking it is still
    // alive first: the shot outlives its own flight time and the enemy can
    // die - to this very shot's owner being cut down mid-flight - long
    // before it lands. See LiveOwner().
    class Enemy* m_Owner = nullptr;

    class Enemy* LiveOwner() const;

    void EmitTrail();
    bool HitPlayer();
    bool HitSolid() const;

public:
    void Init() override;
    void Update() override;

    // The trail is the picture; this object is only a point that moves.
    void Draw() override {}

    // MaxRange is the distance the wave may cover before it burns out.
    void Fire(class Enemy* Owner, const Vector3& Position, const Vector3& Direction,
        int Damage, float MaxRange);
};
