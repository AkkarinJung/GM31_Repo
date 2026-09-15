#pragma once

#include "GameObject.h"

// A 2D slash sprite in world space, played as a png frame sequence.
//
// Spawn one per swing and forget it - it plays through its frames and
// destroys itself:
//
//   Manager::AddGameObj<SlashEffect>()->Play(position, angle, 1.6f, 1.6f);
//
// It carries no damage. The hitbox is the weapon's business (see
// Sword::Use) and the two are triggered separately from Player::Update, so
// the visual can be as big as it needs to be without touching the hitbox.
//
// It is a real plane in the world, not a billboard. Position, rotation and
// scale are the GameObject's own m_Position / m_Rotation / m_Scale, built by
// GameObject::GetMatrx() exactly like every other object - so all three
// rotation axes work:
//
//   Rotation.x  pitch - tips the arc towards or away from the camera
//   Rotation.y  yaw   - swings it through depth, which is what sells the
//                       effect as 3D rather than a sticker on the screen
//   Rotation.z  roll  - spins it in the screen plane
//
// The play plane is XY at z=0 and the camera looks down +Z, so a rotation of
// all zeros already faces the camera; the other two axes tilt away from that.
// Back-face culling is turned off while it draws, so no orientation can make
// it disappear.
class SlashEffect : public GameObject
{
private:
    float m_Timer = 0.0f;
    float m_Lifetime = 0.14f;   // the only place a duration lives; the whole
                                // swing is fitted into it. Short on purpose -
                                // a slash that lingers stops reading as fast

    float m_StartAlpha = 1.0f;

    // How far the streak rotates over its life, in radians. The sign decides
    // which way the blade travels, so the caller flips it when the character
    // turns around.
    float m_Sweep = 1.1f;

    // What Play() was handed. The stretch and the sweep are measured from
    // these every frame, so they can never accumulate drift.
    Vector3 m_BaseScale{ 1.0f, 1.0f, 1.0f };
    float m_BaseRoll = 0.0f;

    // The swing already dies on its own alpha curve, so this extra linear
    // fade is off. Turn it on for a sprite that needs to be faded by hand.
    bool m_FadeOut = false;

    // trail.png is a white blob, so additive is what turns it into light
    // rather than a grey smear. Switch it off for a sprite that carries its
    // own colour.
    bool m_Additive = true;

    // Off so the slash always reads on top of whatever it is cutting -
    // the same choice Particle makes.
    bool m_DepthTest = false;

    // Optional: ride the weapon instead of standing where it was spawned.
    //
    // A slash spawned once and left there has to guess the swing's angle from
    // a single instant, and the instant available - the frame the hit lands -
    // is one where the blade is still behind the player's back on two of the
    // three combo steps. Tracking it every frame removes the guess entirely:
    // starting behind him is then correct, because the arc sweeps forward
    // from there along with the blade.
    class GameObject* m_Follow = nullptr;
    float m_FollowReach = 0.6f;  // how far up the blade the arc centres
    float m_FollowDepth = 0.0f;  // world z nudge, to keep it off the body

    // The last usable roll. A blade pointing almost straight into the screen
    // has no meaningful on-screen angle, so the arc holds the last one it had
    // rather than snapping to whatever rounding produced.
    float m_FollowRoll = 0.0f;
    bool m_HasFollowRoll = false;
    const float m_FollowRollMin = 0.25f;

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    // Position is the centre of the slash, in world space.
    // Rotation is pitch/yaw/roll in radians - the same convention every other
    // GameObject uses (XMMatrixRotationRollPitchYaw).
    // Scale is half-extents in world units, so the quad spans 2*Scale.x by
    // 2*Scale.y; z is unused by a flat quad but kept for consistency.
    // Lifetime is in seconds and the frame sequence is stretched to fill it.
    void Play(const Vector3& Position, const Vector3& Rotation, const Vector3& Scale,
        float Lifetime = 0.14f, float Alpha = 1.0f, float Sweep = 1.1f);

    // Ride the weapon for the rest of this slash's life. Cancels the sweep -
    // the blade's own movement IS the sweep once this is on.
    void FollowWeapon(class GameObject* Weapon, float Reach, float Depth);

    void SetAdditive(bool Additive) { m_Additive = Additive; }
    void SetDepthTest(bool DepthTest) { m_DepthTest = DepthTest; }
    void SetFadeOut(bool FadeOut) { m_FadeOut = FadeOut; }

    // The texture is shared by every slash and loaded once. Call this from a
    // scene's Init to get the loading over with up front - otherwise the
    // first swing of the stage pays for it mid-swing.
    static void LoadShared();

    // Releases them. Call once at shutdown, from Manager::Uninit.
    static void UninitShared();
};
