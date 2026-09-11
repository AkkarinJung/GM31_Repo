#pragma once

#include "GameObject.h"
#include "animationModel.h"

class Player : public GameObject
{
private:
    Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f }; //‘¬“x

    ID3D11InputLayout* m_VertexLayout;
    ID3D11VertexShader* m_VertexShader;
    ID3D11PixelShader* m_PixelShader;

    bool m_Ground = true;
    float m_MoveAnimation = 0.0f;
    class Audio* m_JumpSE;
    int m_JumpCount = 0;
    int m_MaxJumps = 1; // raise this later for double/triple jump
    float m_JumpPower = 20.0f;

    GameObject* m_Child;
    GameObject* m_Shadow;

    class AnimationModel* m_AnimationModel;
    int m_AnimationFrame = 0;
    std::string m_AnimationName;

    int m_NextAnimationFrame = 0;
    std::string m_NextAnimationName;

    float m_Blend = 0.0f;

    bool m_Attacking = false;
    int m_AttackCombo = 0;
    int m_AttackAnimLength = 0;

    class Stats* m_Stats;

    class BoneAttachPoint* m_WeaponSocket;

    //Vector3 m_IdleWeaponOffsetPos{ -4.3333f, 5.3333f, -4.6667f };
    //Vector3 m_IdleWeaponOffsetRot{ 0.0f, 4.2333f, -1.8f };
    Vector3 m_IdleWeaponOffsetPos{ 0.0000f, 0.0000f, 0.0000f };
    Vector3 m_IdleWeaponOffsetRot{ 0.0000f, 0.0000f, 0.0000f };
    class Sword* m_Weapon;

    bool m_FreezeAnimation = false;

    Vector3 m_AttackOffsetPos[3][3]{
        // Attack1
        { { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f, 0.0000f } },
        // Attack2
        { { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f, 0.0000f } },
        // Attack3
        { { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f, 0.0000f } }
    };
    Vector3 m_AttackOffsetRot[3][3]{
        // Attack1
        { { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f, 0.0000f } },
        // Attack2
        { { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f, 0.0000f } },
        // Attack3
        { { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f, 0.0000f } }
    };

    //Vector3 m_AttackOffsetPos[3][3]{
    //    // Attack1
    //    { { 0.0f, 0.0f, 0.0f }, { -7.6667f, 6.3333f, -17.0000f }, { -7.0000f, 9.0000f, -4.6667f } },
    //    // Attack2
    //    { { 2.3334f, 10.0000f, -3.6667f }, { -7.6667f, 6.3333f, -17.0000f }, { -5.6667f, 2.6667f, -2.6667f } },
    //    // Attack3
    //    { { 2.3334f, 7.3333f, -3.6667f }, { -2.0000f, 7.0000f, -12.0000f }, { -5.6667f, 2.6667f, -2.6667f } }
    //};
    //Vector3 m_AttackOffsetRot[3][3]{
    //    // Attack1
    //    { { 0.0f, 0.0f, 0.0f }, { -2.7333f, -2.2667f, 4.0000f }, { -10.8334f, 5.8666f, -9.0333f } },
    //    // Attack2
    //    { { -0.3333f, -4.1666f, -1.6000f }, { -2.7333f, -2.2667f, 0.1667f }, { -6.2000f, 0.1333f, -1.3000f } },
    //    // Attack3
    //    { { 5.6000f, -9.2333f, -1.9000f }, { -4.0666f, -7.9334f, -4.5000f }, { -11.1000f, 1.3333f, -1.3000f } }
    //};

    int m_TuningKeyframeIndex = 0; // which of the 3 keyframes the tuning keys currently edit

    float m_ComboResetTimer = 999.0f; // starts "expired" so the very first attack begins at Attack1
    const float m_ComboWindow = 0.6f; // seconds allowed between attacks to keep the combo going

    bool m_AttackQueued = false;

    //Right attack
    bool m_UsingRightAttack = false;
    int m_RightAttackMPCost = 15;

    Vector3 m_RightAttackOffsetPos[3]{
       { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f,0.0000f,0.0000f }
    };
    Vector3 m_RightAttackOffsetRot[3]{
       { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f, 0.0000f }, { 0.0000f, 0.0000f,0.0000f }
    };

    void StartAttack();
    void UpdateAttackWeaponOffset();
    void StartRightAttack();

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    void SetAnimation(const char* AnimationName);

    void DebugDumpSwing(const char* AnimationName, const char* BoneName);
    Vector3 SlerpRotation(const Vector3& RotA, const Vector3& RotB, float T);
};