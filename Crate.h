#pragma once

#include "GameObject.h"

// A breakable crate - the only thing in the game that drops a pickup.
//
// Not to be confused with Box, which is the static platform the stages are
// built out of. The stage table calls those "crates" too, but they are
// scenery with collision and nothing else: they have no HP and the sword
// passes straight through them. This one answers the sword, and rolls on the
// drop table when it breaks. See Break() for the table itself.
class Crate : public GameObject
{
private:
    class AnimationModel* m_Model = nullptr;

    // One swing breaks it. A crate is a prop, not a fight - the interesting
    // part is what falls out, and an HP sponge in the middle of the floor
    // only slows the run down. Raise it if a crate should take a beating.
    int m_HP = 1;

    // Break() spawns a potion, so it must run exactly once. The swing's hit
    // window is several frames long and Sword only tracks what it has hit
    // per swing, so a second swing arriving before the object is actually
    // deleted would otherwise roll the table again.
    bool m_Broken = false;

    // Maps the model as the artist built it onto the unit crate the
    // collision assumes, measured rather than hard coded - exactly the way
    // Box does it, and for the same reason. See Box::Init.
    Vector3 m_FitScale{ 1.0f, 1.0f, 1.0f };
    Vector3 m_FitOffset{ 0.0f, 0.0f, 0.0f };

    // Hit wobble. A DRAW offset and nothing else - m_Position is never
    // written by it, which is the rule Enemy's shake had to learn the hard
    // way (see the comment on Enemy::m_Shake).
    Vector3 m_Shake{ 0.0f, 0.0f, 0.0f };
    Vector3 m_ShakeOffset{ 0.0f, 0.0f, 0.0f };
    int m_ShakeFlip = 0;
    const float m_ShakeDecay = 0.75f;

    void Break();

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    // Critical only reaches the damage number - the extra damage is already
    // in Damage by the time it gets here, the same contract Enemy uses.
    void AddDamage(int Damage, bool Critical = false);

    void Shake(Vector3 Shake)
    {
        m_Shake = Shake;
        m_ShakeOffset = Shake;
        m_ShakeFlip = 0;
    }

    // What the sword swings at. Answered the same way Enemy answers it, so
    // the one 2.5D reach test in Sword::Use serves both without a special
    // case for either.
    float GetHitRadius() const { return m_Scale.x; }
    float GetBodyHeight() const { return m_Scale.y * 2.0f; }
};
