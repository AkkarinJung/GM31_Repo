#pragma once
#include "Weapon.h"

class Sword : public Weapon
{
private:
    //class ModelRenderer* m_ModelRenderer = nullptr;
    class AnimationModel* m_AnimationModel = nullptr;

    float m_AngleDot = 0.5f; // cos of half the swing arc - enemies in front only

protected:
    void LoadModel() override;

public:
    void Use(GameObject* Owner) override;
};
