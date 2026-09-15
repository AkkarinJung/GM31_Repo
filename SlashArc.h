#pragma once

#include "GameObject.h"

// The anime slash arc - a crescent built out of GEOMETRY, not out of a
// sprite.
//
// This is deliberately not SlashEffect. That one draws a flat quad and gets
// its curve from the artwork on it, which makes the effect only ever as good
// as the sheet's cell layout and leaves nothing about the shape available to
// code. Here the crescent is generated at load time from four numbers, so:
//
//   * the curve is real geometry - it can be bowed, lengthened and tapered
//     independently, and no UV or cell size can distort it;
//   * the bright core and the soft outer fade come from VERTEX COLOUR across
//     the ribbon's width, so there is no texture to load, mip, or get wrong;
//   * it can WIPE ON, leading tip first, by drawing a growing slice of the
//     same buffer. That is the part a single sprite cannot do at all, and it
//     is what makes the arc read as a cut being made rather than as a picture
//     of a cut appearing.
//
// It carries no damage. The hitbox is Sword::Use, driven from a different
// place in Player::Update, so this can be any size at all without touching
// what a swing hits.
//
//   Manager::AddGameObj<SlashArc>()->Play(pos, angle, length, bow, life, sweep);
//
// It plays once and destroys itself, so nothing can leave one behind.
//
// 2.5D. The crescent is generated in the XY plane with its chord along X and
// its bow along +Y, and it is aimed by rolling about Z - the plane the game
// is played on and the one the camera looks straight at down +Z. There is no
// billboard and no pitch/yaw guesswork: the only angle that exists is the one
// the caller passes in, taken from the player's actual facing. Back-face
// culling is off while it draws, so no roll can make it vanish.
class SlashArc : public GameObject
{
private:
    float m_Timer = 0.0f;
    float m_Lifetime = 0.16f;

    // How far the arc rotates over its life, in radians. The caller flips the
    // sign with the facing so the cut always travels the way the character is
    // swinging.
    float m_Sweep = 0.5f;

    // What Play() was handed. Every frame's shape is measured from these, so
    // nothing can accumulate drift over the life of the arc.
    float m_BaseRoll = 0.0f;
    float m_BaseLength = 1.0f;
    float m_BaseBow = 1.0f;

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    // Position  centre of the cut, world space.
    // Angle     which way the cut points, radians, rolled about Z. 0 lays the
    //           chord along +X; the caller derives it from the player's facing
    //           and which step of the combo this is.
    // Length    half the chord, in world units - how long the cut is.
    // Bow       how far the crescent bellies out from that chord.
    // Lifetime  seconds. Short: a slash that lingers stops reading as fast.
    // Sweep     radians of rotation over that life, signed.
    void Play(const Vector3& Position, float Angle, float Length, float Bow,
        float Lifetime, float Sweep);

    // Generates the crescent once and keeps it. Call from a scene's Init to
    // get it over with up front.
    static void LoadShared();
    static void UninitShared();
};
