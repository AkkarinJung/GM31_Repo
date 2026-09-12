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
    float m_AttackBufferTimer = 0.0f; // a press is remembered this long, then dropped

    // Where in the swing the blade is actually dangerous, as a fraction of
    // the animation. Damage used to land the instant the button went down,
    // with the sword still behind the player - this is what made the hits
    // feel disconnected from the animation.
    const float m_AttackHitPoint = 0.35f;
    bool m_AttackHitDone = false;

    // How far into a swing the next one may start. Waiting for the full
    // animation (recovery included) is what made combos feel sluggish.
    const float m_ComboCancelPoint = 0.6f;
    const float m_AttackBufferTime = 0.35f;

    // A short freeze on impact, then the camera kick - the two cheapest
    // things that make a hit read as a hit.
    int m_HitStopFrames = 0;
    const int m_HitStopOnHit = 5;
    const float m_HitShake = 0.06f;

    // A small step into the swing, so an attack has weight behind it.
    const float m_AttackLunge = 3.0f;

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