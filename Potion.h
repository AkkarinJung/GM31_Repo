#pragma once

#include "GameObject.h"

// Which bar the potion fills. The model is picked from this too - red for
// health, blue for mana - so the colour on the floor always matches what
// picking it up does.
enum class PotionType
{
    Health,
    Mana,
};

// A pickup thrown out of a breaking crate (see Crate::Break).
//
// It pops out, falls under the same gravity the player uses and lands on
// whatever is beneath it - the ground or the top of a platform - then bobs
// in place until the player walks into it. There is no timer: a potion the
// player has not reached yet is still theirs, and the scene is rebuilt
// between stages anyway, so nothing leaks from one map into the next.
//
// Walking into it STORES it in PotionBag rather than drinking it - the
// player chooses when to spend it. If both slots are full the potion is
// left exactly where it is, so coming back for it after drinking one always
// works.
class Potion : public GameObject
{
private:
    PotionType m_Type = PotionType::Health;
    class ModelRenderer* m_Model = nullptr;

    Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
    bool m_Grounded = false;

    // Drives the idle bob and spin once it has landed.
    float m_IdleTime = 0.0f;

    // Stops a potion being collected on the same frame it is created, while
    // it is still inside the crate that dropped it and the player is stood
    // right there having just swung. Without it the pickup is invisible -
    // the crate bursts and the bar simply moves.
    float m_PickupDelay = 0.0f;

    bool m_Collected = false; // so one frame cannot store the potion twice

    void TryCollect();

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    // Called straight after AddGameObj, the way Polygon2D and DamageNumber
    // are set up. Loads the model for the type, so the type has to be known
    // before this returns.
    void Spawn(PotionType Type, const Vector3& Position, const Vector3& Velocity);
};
