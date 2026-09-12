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
    // Movement tuning. Fields rather than literals in Update() so the
    // start-of-map rewards can scale them (see RoguelikeSystem).
    float m_MoveSpeed = 50.0f;
    float m_JumpPower = 20.0f;
    class Audio* m_JumpSE;

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

    // How the sword sits in the hand, in bone space. One offset for every
    // state: the grip does not change because the arm swings, so the socket
    // is set once in Init() and left alone. Tune it live with the keys in
    // Update(), then press P and copy the numbers back here.
    Vector3 m_WeaponOffsetPos{ -4.6667f, 7.6667f, 0.0000f };
    Vector3 m_WeaponOffsetRot{ 0.0000f, 0.0000f, -1.0000f };
    class Sword* m_Weapon;

    bool m_FreezeAnimation = false;

    float m_ComboResetTimer = 999.0f; // starts "expired" so the very first attack begins at Attack1
    const float m_ComboWindow = 0.6f; // seconds allowed between attacks to keep the combo going

    bool m_AttackQueued = false;

    //Right attack
    int m_RightAttackMPCost = 15;

    void StartAttack();
    void StartRightAttack();

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    void SetAnimation(const char* AnimationName);
    // Read/written by the roguelike rewards.
    float GetMoveSpeed() const { return m_MoveSpeed; }
    void SetMoveSpeed(float MoveSpeed) { m_MoveSpeed = MoveSpeed; }

    float GetJumpPower() const { return m_JumpPower; }
    void SetJumpPower(float JumpPower) { m_JumpPower = JumpPower; }

    class Weapon* GetWeapon() const;

    void DebugDumpSwing(const char* AnimationName, const char* BoneName);
};