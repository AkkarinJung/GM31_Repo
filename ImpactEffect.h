#pragma once

#include "GameObject.h"

// The burst where a swing actually connects.
//
// Spawned per CONFIRMED HIT, never per swing - Player asks the weapon what it
// landed on this frame and puts one of these on each. A swing that misses
// still shows its trail and its arc and nothing else, which is exactly how a
// miss should read.
//
// Like SlashEffect and SwordTrail it carries no damage. It is told where the
// hit was and which way the blow travelled; it never decides either.
//
//   Manager::AddGameObj<ImpactEffect>()->Burst(point, direction, 1.0f, false);
//
// It plays and destroys itself. Nothing has to hold a pointer to it, which is
// what keeps a burst from outliving the swing, the player or the scene.
//
// Three parts, all from the one soft blob texture:
//
//   flash    a round core that snaps to full size and collapses
//   lines    two short tapered streaks crossing at the contact point,
//            angled off the blow - the "cut" marks
//   sparks   a fan of thin streaks thrown out along the blow, slowing as
//            they go
//
// 2.5D. Everything is built on the world XY plane and rolled about Z, the
// same plane SlashEffect draws on and the same plane the game is played on.
// The camera sits back down -Z looking at it, so nothing here can turn
// edge-on, and culling is off while it draws so nothing can be culled either.
class ImpactEffect : public GameObject
{
private:
    // One thrown streak.
    struct Spark
    {
        float Angle = 0.0f;   // direction of travel, radians in the XY plane
        float Speed = 0.0f;   // world units a second at t=0
        float Length = 0.0f;  // half-extents of its quad, at t=0
        float Width = 0.0f;
    };

    // ParticleCount's ceiling. A hit that lands on three enemies at once puts
    // three of these on screen, so this is really "sparks x targets" - small
    // on purpose.
    static const int SPARK_MAX = 8;

    Spark m_Spark[SPARK_MAX];
    int m_SparkCount = 0;

    float m_Timer = 0.0f;
    float m_Lifetime = 0.22f;

    // ImpactScale - one multiplier over the whole burst, so a heavier swing
    // hits visibly harder without a second set of constants.
    float m_Scale = 1.0f;

    // Which way the blow was travelling, as an angle in the XY plane. The
    // whole burst is built around it: the cut lines straddle it and the spark
    // fan is thrown along it. This is why the effect cannot be facing-blind -
    // it has no fixed orientation of its own at all.
    float m_Angle = 0.0f;

    // A finisher or a critical: bigger, longer, more of everything.
    bool m_Heavy = false;

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    // Position is the contact point in world space. Direction is the way the
    // blow travelled - it only has to point the right way, length is ignored,
    // and a zero vector falls back to +X rather than producing a NaN angle.
    void Burst(const Vector3& Position, const Vector3& Direction,
        float Scale = 1.0f, bool Heavy = false);

    // Shared by every burst and loaded once. Called from Game::Init so the
    // first hit of the stage does not pay for it mid-swing; safe to call
    // again from anywhere.
    static void LoadShared();

    // Released once at shutdown, from Manager::Uninit.
    static void UninitShared();
};
