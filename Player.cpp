#include "main.h"
#include "renderer.h"
#include "Player.h"
//#include "modelRenderer.h"
#include "animationModel.h"
#include "input.h"
#include "Collision.h"

#include "Manager.h"
#include "Camera.h"
#include "SoundEffect.h"

#include "Tree.h"
#include "Shadow.h"
#include "MeshField.h"

#include "Stats.h"

#include "BoneAttachPoint.h"
#include "Sword.h"
#include "SlashEffect.h"

void Player::Init()
{
    m_Layer = 1;
    m_Position = { 0.0f, 0.0f, 0.0f };
    m_Scale = { 0.01f, 0.01f, 0.01f };

    //ModelRenderer* m_ModelRenderer = AddGameComponent<ModelRenderer>(this);
    //m_ModelRenderer->Load("asset\\model\\player.obj");
    m_AnimationModel = AddGameComponent<AnimationModel>(this);
    m_AnimationModel->Load("asset\\model\\Player_Movement\\Idle_model.fbx");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Movement\\Idle_model.fbx", "Idle");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Movement\\Run.fbx", "Run");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Movement\\Jump.fbx", "Jump");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Attack\\Attack_1.fbx", "Attack1");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Attack\\Attack_2.fbx", "Attack2");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Attack\\Attack_3.fbx", "Attack3");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Attack\\Attack_Right.fbx", "AttackRight");
    m_AnimationModel->DebugPrintBoneNames();

    m_AnimationName = "Idle";
    m_NextAnimationName = "Idle";

    m_WeaponSocket = AddGameComponent<BoneAttachPoint>(this);
    m_WeaponSocket->SetBone(m_AnimationModel, "mixamorig:RightHand");
    m_WeaponSocket->SetLocalTransform(m_WeaponOffsetPos, m_WeaponOffsetRot, { 1.0f, 1.0f, 1.0f });
    m_Weapon = Manager::AddGameObj<Sword>();
    m_WeaponSocket->Attach(m_Weapon);

    // シェーダー読込
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\litTextureVS.cso");

    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\litTexturePS.cso");

    m_Shadow = Manager::AddGameObj<Shadow>();
    m_Shadow->SetScale({ 1.5f ,1.5f ,1.5f });

    m_Stats = AddGameComponent<Stats>(this);
    m_Stats->SetMaxHP(100);
    m_Stats->SetAttack(5); // base unarmed attack - weapons add on top of this
    m_Stats->SetDefense(2);
}

void Player::Uninit()
{
    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();

    GameObject::Uninit();
}

void Player::Update()
{
    // 1/2/3 jump to the start / middle / end of the current swing, which is
    // how you check the grip at the extremes of the animation (F2 freezes).
    if (Input::GetKeyTrigger('1') && m_Attacking) { m_NextAnimationFrame = 0; m_Blend = 1.0f; }
    if (Input::GetKeyTrigger('2') && m_Attacking) { m_NextAnimationFrame = m_AttackAnimLength / 2; m_Blend = 1.0f; }
    if (Input::GetKeyTrigger('3') && m_Attacking) { m_NextAnimationFrame = m_AttackAnimLength - 1; m_Blend = 1.0f; }

    if (Input::GetKeyTrigger(VK_F2))
        m_FreezeAnimation = !m_FreezeAnimation;

    if (m_FreezeAnimation && Input::GetKeyTrigger(VK_F3))
    {
        m_AnimationFrame++;
        m_NextAnimationFrame++;
    }

    float dt = 1.0 / 60.0f;

    if (Input::GetKeyTrigger(VK_F4))
        DebugDumpSwing("Attack1", "mixamorig:RightHand");

    bool oldGround = m_Ground;
    m_Ground = false;

    // 2.5D side-scroll movement: only left/right along world X - no
    // depth/forward-back input, since the camera no longer rotates to
    // face any other direction. Locked out while attacking.
    bool move = false;

    if (!m_Attacking)
    {
        if (Input::GetKeyPress('D'))
        {
            m_Velocity.x += m_MoveSpeed * dt;
            move = true;
        }
        if (Input::GetKeyPress('A'))
        {
            m_Velocity.x -= m_MoveSpeed * dt;
            move = true;
        }
    }

    float tuneStep = 20.0f * dt;
    float tuneRotStep = 2.0f * dt;

    // The socket holds the offset now, in every state - nothing overwrites
    // it per frame, so tuning works while idle, mid-swing or frozen alike.
    if (Input::GetKeyPress('H')) m_WeaponSocket->AdjustLocalPosition({ -tuneStep, 0.0f, 0.0f });
    if (Input::GetKeyPress('K')) m_WeaponSocket->AdjustLocalPosition({ tuneStep, 0.0f, 0.0f });
    if (Input::GetKeyPress('N')) m_WeaponSocket->AdjustLocalPosition({ 0.0f, -tuneStep, 0.0f });
    if (Input::GetKeyPress('U')) m_WeaponSocket->AdjustLocalPosition({ 0.0f, tuneStep, 0.0f });
    if (Input::GetKeyPress('G')) m_WeaponSocket->AdjustLocalPosition({ 0.0f, 0.0f, -tuneStep });
    if (Input::GetKeyPress('T')) m_WeaponSocket->AdjustLocalPosition({ 0.0f, 0.0f, tuneStep });
    if (Input::GetKeyPress('Z')) m_WeaponSocket->AdjustLocalRotation({ -tuneRotStep, 0.0f, 0.0f });
    if (Input::GetKeyPress('X')) m_WeaponSocket->AdjustLocalRotation({ tuneRotStep, 0.0f, 0.0f });
    if (Input::GetKeyPress('C')) m_WeaponSocket->AdjustLocalRotation({ 0.0f, -tuneRotStep, 0.0f });
    if (Input::GetKeyPress('V')) m_WeaponSocket->AdjustLocalRotation({ 0.0f, tuneRotStep, 0.0f });
    if (Input::GetKeyPress('B')) m_WeaponSocket->AdjustLocalRotation({ 0.0f, 0.0f, -tuneRotStep });
    if (Input::GetKeyPress('M')) m_WeaponSocket->AdjustLocalRotation({ 0.0f, 0.0f, tuneRotStep });

    if (Input::GetKeyTrigger('P'))
        m_WeaponSocket->DebugPrintTransform();

    DebugTuneSlash();
    TrackWeaponMotion();

    // Only re-aim while actually moving. atan2f(0, 0) is 0, so reading the
    // facing every frame snapped the player (and the sword parented to it)
    // round to face +Z the moment the velocity died out, and flipped it
    // 180 degrees on the frame the velocity crossed zero.
    // Not while attacking either: the lunge and the recoil are velocity too,
    // and reading the facing off them spun the player away from the enemy
    // mid-swing.
    if (!m_Attacking && (fabsf(m_Velocity.x) > 0.01f || fabsf(m_Velocity.z) > 0.01f))
        m_Rotation.y = atan2f(m_Velocity.x, m_Velocity.z);

    // oldGround, not m_Ground: m_Ground was cleared at the top of Update and
    // is not recomputed until after the position integration below, so it is
    // always false here. oldGround is what the player stood on last frame -
    // without this check every press adds jump speed, mid-air included.
    if (oldGround && Input::GetKeyTrigger(VK_SPACE))
    {
        m_Velocity.y += m_JumpPower;

        //m_Scale.y = 2.0f;
        //m_Scale.x = 0.5f;
        //m_Scale.z = 0.5f;

        SoundEffect::Play(SE::Jump);
    }

    //return scale to original
    //m_Scale.x += (1.0f - m_Scale.x) * 0.1f;
    //m_Scale.y += (1.0f - m_Scale.y) * 0.1f;
    //m_Scale.z += (1.0f - m_Scale.z) * 0.1f;

    m_Velocity.y += -98.0f * dt;

    m_Velocity.x += -m_Velocity.x * 5.0f * dt;
    m_Velocity.z += -m_Velocity.z * 5.0f * dt;

    // Solids first, one axis at a time. Resolving X and Y separately is what
    // keeps a corner from producing two fighting pushes in the same frame.
    std::vector<AABB> solids = Collision::GatherSolids();

    if (Collision::MoveX(m_Position, m_BodyHalfSize, m_Velocity.x * dt, solids))
        m_Velocity.x = 0.0f;

    bool landed = false;
    if (Collision::MoveY(m_Position, m_BodyHalfSize, m_Velocity.y * dt, solids, landed))
        m_Velocity.y = 0.0f;

    if (landed)
        m_Ground = true;

    MeshField* meshField = Manager::GetGameObj<MeshField>();
    float height = meshField->GetHeight(m_Position);

    if (m_Position.y < height)
    {
        m_Position.y = height;
        m_Velocity.y = 0.0f;
        m_Ground = true;
    }

    auto trees = Manager::GetGameObjs<Tree>();
    for (auto tree : trees)
    {
        Vector3 treePos = tree->GetPosition();
        Vector3 playerPos = m_Position;

        treePos.y = 0.0f;
        playerPos.y = 0.0f;

        Vector3 dir = playerPos - treePos;
        float length = VectorMag(dir);
        if (length < 1.4f)
        {
            dir /= length;
            dir *= 1.4f - length;

            m_Position += dir;
        }
    }

    // Enemies are deliberately not resolved here. Nothing but input, gravity
    // and solid geometry is allowed to move the player: an enemy that walks
    // into the player steps out of the player instead (see Enemy::Update).
    // Pushing from this side meant two enemies either side each shoved the
    // player at the other one every frame, which is what made it jitter.

    // Solids get the last word - the tree push above is soft and could have
    // put the player inside a crate.
    Collision::PushOutOfSolids(m_Position, m_BodyHalfSize, solids);


    //if (!oldGround && m_Ground)
    //{
    //    m_Scale.y = 0.5f;
    //    m_Scale.x = 2.0f;
    //    m_Scale.z = 2.0f;
    //}

    if (m_ParryTimer > 0.0f)
        m_ParryTimer -= dt;

    // MP trickles back. The special costs 15 and a parry only refunds 10, so
    // without this the bar empties and right click quietly does nothing.
    if (m_Stats != nullptr && m_Stats->GetMP() < m_Stats->GetMaxMP())
    {
        m_MPRegenCarry += m_MPRegenPerSecond * dt;

        int whole = (int)m_MPRegenCarry;
        if (whole > 0)
        {
            m_Stats->RestoreMP(whole);
            m_MPRegenCarry -= (float)whole;
        }
    }

    // Cancels out of a normal swing. Requiring !m_Attacking silently ate the
    // press whenever the player was mid-combo, which is exactly when an enemy
    // swing is coming at them - it looked like the parry did nothing. The
    // weapon cooldown is not checked either: this is a defensive move, and
    // its own damage still goes through the hit window later.
    if (Input::GetKeyTrigger(VK_RBUTTON) && !m_SpecialAttacking)
    {
        StartRightAttack();
    }

    // Every press is buffered and spends itself as soon as the swing allows
    // it, instead of being dropped for arriving a few frames early.
    if (Input::GetKeyTrigger(VK_LBUTTON))
    {
        m_AttackQueued = true;
        m_AttackBufferTimer = m_AttackBufferTime;
    }

    if (m_AttackQueued)
    {
        m_AttackBufferTimer -= dt;
        if (m_AttackBufferTimer <= 0.0f)
            m_AttackQueued = false; // stale press, let it go
    }

    // The swing lands here, partway through the animation, not when the
    // button went down.
    if (m_Attacking && !m_AttackHitDone && m_AttackAnimLength > 0 &&
        m_NextAnimationFrame >= (int)(m_AttackAnimLength * m_AttackHitPoint))
    {
        m_AttackHitDone = true;

        // Visual and hitbox are separate on purpose. The slash shows on
        // every swing, hit or miss, and may reach further than the sword
        // actually does - changing how it looks can never change what it
        // damages.
        SpawnSlash();

        if (m_Weapon->Use(this))
        {
            // Connected: hold the frame for a moment and kick the camera.
            m_HitStopFrames = m_HitStopOnHit;
            SoundEffect::Play(SE::SwordHit);

            Camera* camera = Manager::GetGameObj<Camera>();
            if (camera != nullptr)
                camera->Shake(GetFoward() * m_HitShake);
        }
    }

    if (m_Attacking && m_NextAnimationFrame >= m_AttackAnimLength)
    {
        m_Attacking = false;
        m_SpecialAttacking = false;
    }

    // Start the next swing: either the player is idle, or the current swing
    // has gone past its cancel point - a queued press never has to wait for
    // the recovery frames to play out. CanUse() keeps a swing from starting
    // at all when the weapon could not damage anything, so there are no
    // empty swings.
    if (m_AttackQueued && m_Weapon->CanUse() && !IsParrying())
    {
        bool canStart = !m_Attacking ||
            (m_AttackHitDone &&
                m_NextAnimationFrame >= (int)(m_AttackAnimLength * m_ComboCancelPoint));

        if (canStart)
        {
            m_AttackQueued = false;
            StartAttack();
        }
    }

    if (!m_Attacking)
    {
        m_ComboResetTimer += dt;
    }

    // keep the player pinned to the scrolling plane - collision push-out
    // from trees/boxes/enemies (which resolve in X/Z) could otherwise
    // drift the player off Z=0 over time.
    m_Position.z = 0.0f;

    Vector3 shadowPos = m_Position;
    shadowPos.y = 0.01f;
    m_Shadow->SetPosition(shadowPos);

    if (!m_Attacking)
    {
        if (!m_Ground)
        {
            SetAnimation("Jump");
        }
        else if (move)
        {
            SetAnimation("Run");
        }
        else
        {
            SetAnimation("Idle");
        }
    }

    // The frame the player touches down. oldGround is last frame's state and
    // m_Ground has been recomputed by the collision pass above, so this fires
    // once per landing rather than every frame on the floor.
    if (!oldGround && m_Ground && m_Velocity.y <= 0.0f)
        SoundEffect::Play(SE::Land);

    if (m_HitStopFrames > 0)
    {
        // Impact freeze - the animation holds on the contact pose for a few
        // frames. Nothing else about the player is paused, so it reads as
        // weight rather than as a stutter.
        m_HitStopFrames--;
    }
    else if (!m_FreezeAnimation)
    {
        m_AnimationFrame++;
        m_NextAnimationFrame++;
        m_Blend += 0.1f;
        if (m_Blend > 1.0f)
            m_Blend = 1.0f;
    }

    m_AnimationModel->Update(m_AnimationName.c_str(), m_AnimationFrame, m_NextAnimationName.c_str(), m_NextAnimationFrame, m_Blend);

    GameObject::Update();
}

void Player::Draw()
{
    // 入力レイアウト設定
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

    // シェーダ設定
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    GameObject::Draw();
}

void Player::SetAnimation(const char* AnimationName)
{
    if (m_NextAnimationName != AnimationName)
    {
        m_AnimationName = m_NextAnimationName;
        m_AnimationFrame = m_NextAnimationFrame;

        m_NextAnimationName = AnimationName;
        m_NextAnimationFrame = 0;

        m_Blend = 0.0f;
    }
}

Weapon* Player::GetWeapon() const
{
    return m_Weapon;
}

void Player::DebugDumpSwing(const char* AnimationName, const char* BoneName)
{
    int frameCount = m_AnimationModel->GetAnimationFrameCount(AnimationName);

    char buffer[256];
    sprintf_s(buffer, "=== %s / %s (%d frames) ===\n", AnimationName, BoneName, frameCount);
    OutputDebugStringA(buffer);

    for (int f = 0; f < frameCount; f++)
    {
        m_AnimationModel->Update(AnimationName, f, AnimationName, f, 1.0f);

        XMMATRIX boneMatrix;
        if (!m_AnimationModel->GetBoneMatrix(BoneName, &boneMatrix))
            continue;

        XMVECTOR scale, rotQuat, translation;
        XMMatrixDecompose(&scale, &rotQuat, &translation, boneMatrix);

        XMFLOAT3 pos;
        XMStoreFloat3(&pos, translation);

        XMFLOAT4 quat;
        XMStoreFloat4(&quat, rotQuat);

        sprintf_s(buffer, "frame %2d: pos(%.4f, %.4f, %.4f) quat(%.4f, %.4f, %.4f, %.4f)\n",
            f, pos.x, pos.y, pos.z, quat.x, quat.y, quat.z, quat.w);
        OutputDebugStringA(buffer);
    }
}

bool Player::IsParrying() const
{
    return m_ParryTimer > 0.0f;
}

bool Player::TryParry(GameObject* Attacker)
{
    if (!IsParrying())
        return false;

    // Turn on the attacker. The swing is already playing, and finishing it
    // pointed away from whoever was parried looked like a miss.
    if (Attacker != nullptr)
    {
        float dx = Attacker->GetPosition().x - m_Position.x;
        if (fabsf(dx) > 0.01f)
            m_Rotation.y = atan2f(dx, 0.0f);
    }

    m_HitStopFrames = m_ParryHitStop;
    SoundEffect::Play(SE::Parry);

    Camera* camera = Manager::GetGameObj<Camera>();
    if (camera != nullptr)
        camera->Shake(GetFoward() * m_ParryShake);

    if (m_Stats != nullptr)
        m_Stats->RestoreMP(m_ParryMPReward);

    return true;
}

// Where the sword is, sampled every frame, so the swing direction can be
// measured from it. BoneAttachPoint runs at the end of this Update, so what
// is read here is last frame's pose - consistent frame to frame, which is
// all a velocity needs.
void Player::TrackWeaponMotion()
{
    if (m_Weapon == nullptr)
        return;

    // GetMatrx() folds in the parent, so this is a real world position. The
    // sword's own m_Position is in the model's space and would move by
    // ~100x as much.
    XMMATRIX swordWorld = m_Weapon->GetMatrx();

    Vector3 position;
    XMStoreFloat3((XMFLOAT3*)&position, swordWorld.r[3]);

    if (m_HasWeaponHistory)
    {
        Vector3 delta = position - m_PrevWeaponPos;

        // Smoothed, so one noisy frame - or the frame an animation switches
        // and the hand teleports - cannot throw the arc sideways.
        m_WeaponVelocity = m_WeaponVelocity * 0.5f + delta * 0.5f;
    }
    else
    {
        m_WeaponVelocity = Vector3(0.0f, 0.0f, 0.0f);
        m_HasWeaponHistory = true;
    }

    m_PrevWeaponPos = position;
}

// Live tuning for the slash placement, in the same spirit as the weapon
// socket keys above: swing, nudge, swing again, and press F5 to print the
// numbers so they can be pasted back into Player.h. Nothing here affects
// damage - the hitbox is Sword::Use and never reads any of it.
//
//   position   I / O   forward       J / L   height      8 / 9   depth (Z)
//   rotation   4 / 5   roll          Y / 0   yaw         F6 / F7 pitch
//   size       6 / 7   length        F8 / F9 thickness
//   F5         print the current values
//   F10        cycles how the swing angle is found:
//                0 fixed table   1 chest->sword   2 sword velocity
//   F11        toggles the per-swing [SlashSpawn] log
void Player::DebugTuneSlash()
{
    const float dt = 1.0f / 60.0f;
    const float moveStep = 1.5f * dt;
    const float angleStep = 1.5f * dt;
    const float scaleStep = 1.0f * dt;

    if (Input::GetKeyPress('I')) m_SlashForward -= moveStep;
    if (Input::GetKeyPress('O')) m_SlashForward += moveStep;
    if (Input::GetKeyPress('J')) m_SlashHeight -= moveStep;
    if (Input::GetKeyPress('L')) m_SlashHeight += moveStep;
    if (Input::GetKeyPress('8')) m_SlashDepth -= moveStep;
    if (Input::GetKeyPress('9')) m_SlashDepth += moveStep;

    if (Input::GetKeyPress('4')) m_SlashRollTune -= angleStep;
    if (Input::GetKeyPress('5')) m_SlashRollTune += angleStep;
    if (Input::GetKeyPress('Y')) m_SlashYawTune -= angleStep;
    if (Input::GetKeyPress('0')) m_SlashYawTune += angleStep;
    if (Input::GetKeyPress(VK_F6)) m_SlashPitchTune -= angleStep;
    if (Input::GetKeyPress(VK_F7)) m_SlashPitchTune += angleStep;

    if (Input::GetKeyPress('6')) m_SlashLength -= scaleStep;
    if (Input::GetKeyPress('7')) m_SlashLength += scaleStep;
    if (Input::GetKeyPress(VK_F8)) m_SlashThickness -= scaleStep * 0.4f;
    if (Input::GetKeyPress(VK_F9)) m_SlashThickness += scaleStep * 0.4f;

    if (m_SlashLength < 0.1f)    m_SlashLength = 0.1f;
    if (m_SlashThickness < 0.02f) m_SlashThickness = 0.02f;

    if (Input::GetKeyTrigger(VK_F10))
        m_SlashAngleMode = (m_SlashAngleMode + 1) % 3;

    if (Input::GetKeyTrigger(VK_F11))
        m_SlashLogSpawn = !m_SlashLogSpawn;

    if (Input::GetKeyTrigger(VK_F5))
    {
        char buffer[256];
        sprintf_s(buffer,
            "[Slash] forward %.4f  height %.4f  depth %.4f"
            "  length %.4f  thickness %.4f  sweep %.4f"
            "  pitch %+.4f  yaw %+.4f  roll %+.4f\n",
            m_SlashForward, m_SlashHeight, m_SlashDepth,
            m_SlashLength, m_SlashThickness, m_SlashSweep,
            m_SlashPitchTune, m_SlashYawTune, m_SlashRollTune);
        OutputDebugStringA(buffer);

        const char* modeName =
            (m_SlashAngleMode == 0) ? "0 TABLE (fixed per combo)" :
            (m_SlashAngleMode == 1) ? "1 SWORD (chest -> sword)" :
                                      "2 MOTION (sword velocity)";

        float vx = m_WeaponVelocity.x;
        float vy = m_WeaponVelocity.y;

        float sx = 0.0f, sy = 0.0f;
        if (m_Weapon != nullptr)
        {
            XMMATRIX swordWorld = m_Weapon->GetMatrx();
            Vector3 swordPos;
            XMStoreFloat3((XMFLOAT3*)&swordPos, swordWorld.r[3]);
            sx = swordPos.x - m_Position.x;
            sy = swordPos.y - (m_Position.y + m_SlashHeight);
        }

        sprintf_s(buffer,
            "[Slash] angle mode %s   (F10 cycles)\n"
            "        sword  offset(%+.3f, %+.3f) len %.3f -> %+.1f deg\n"
            "        motion vel   (%+.3f, %+.3f) len %.3f -> %+.1f deg\n",
            modeName,
            sx, sy, sqrtf(sx * sx + sy * sy), XMConvertToDegrees(atan2f(sy, sx)),
            vx, vy, sqrtf(vx * vx + vy * vy), XMConvertToDegrees(atan2f(vy, vx)));
        OutputDebugStringA(buffer);
    }
}

// Spawns the swing's slash sprite. Called from the hit window, not from
// StartAttack: at the start of a swing the sword is still behind the
// player's back (which is why the damage waits for m_AttackHitPoint too),
// and BoneAttachPoint - a component, so it runs at the END of this
// Update - has not yet moved the sword onto the new animation's first pose.
void Player::SpawnSlash()
{
    // Measured from the player, not from the sword. The sword's own
    // transform is awkward twice over: m_Position on it is in the model's
    // space rather than the world (BoneAttachPoint parents it to the player
    // and lets GetMatrx() fold the 0.01 scale in), and even read correctly
    // the hand sweeps through a wide arc mid-swing, so a sprite centred on
    // it never landed in the same place twice.
    Vector3 forward = GetFoward();

    // All three axes: along the facing, up, and through depth.
    Vector3 position = m_Position;
    position += forward * m_SlashForward;
    position.y += m_SlashHeight;
    position.z += m_SlashDepth;

    float length;
    float thickness;
    float sweep;
    float lifetime;
    float pitch;
    float yaw;
    float roll;

    if (m_SpecialAttacking)
    {
        length = m_SpecialSlashLength;
        thickness = m_SpecialSlashThickness;
        sweep = m_SpecialSlashSweep;
        lifetime = m_SpecialSlashLifetime;
        pitch = m_SpecialSlashPitch;
        yaw = m_SpecialSlashYaw;
        roll = 0.0f;
    }
    else
    {
        int step = m_AttackCombo % 3;

        length = m_SlashLength;
        thickness = m_SlashThickness;
        sweep = m_SlashSweep;
        lifetime = m_SlashLifetime;
        pitch = m_SlashPitch[step];
        yaw = m_SlashYaw[step];
        roll = m_SlashRoll[step];
    }

    pitch += m_SlashPitchTune;
    yaw += m_SlashYawTune;
    roll += m_SlashRollTune;

    // Turning around is a mirror through the YZ plane: yaw negates, pitch is
    // unchanged, and the roll is handled just below.
    float facing = (forward.x < 0.0f) ? -1.0f : 1.0f;

    Vector3 rotation;
    rotation.x = pitch;
    rotation.y = facing * yaw;

    // Roll, measured off the sword rather than guessed.
    //
    // The crescent bulges along its own +X, so pointing that at the blade's
    // direction of travel lines the arc up with the swing. Only the screen
    // plane matters here - the play plane is XY - so the depth component is
    // dropped before taking the angle.
    //
    // Nothing needs mirroring in this branch: if the character turns around,
    // the sword sweeps the other way and the measured direction follows it
    // on its own.
    bool measured = false;

    float logAlong = 0.0f;
    float logUp = 0.0f;
    float logLocal = 0.0f;

    if (m_SlashAngleMode == 1 && m_Weapon != nullptr)
    {
        // Which way the blade points.
        //
        // sword.fbx measures X +/-0.126, Y +/-0.045, Z 0.004..1.093 - the
        // mesh is 4.3x longer down its local Z than anything else, so the
        // blade runs along local +Z. Row 2 of the world matrix is that axis
        // in world space, with the socket's rotation and the player's turn
        // already folded in.
        XMMATRIX swordWorld = m_Weapon->GetMatrx();

        XMFLOAT3 blade;
        XMStoreFloat3(&blade, XMVector3Normalize(swordWorld.r[2]));

        // Into the character's own frame, so one set of numbers describes a
        // swing whichever way he is turned. Without this a blade pointing at
        // world -X means "out in front" facing left and "wound up behind"
        // facing right, and those want opposite arcs.
        float along = blade.x * facing;  // + = pointing in front
        float up = blade.y;

        logAlong = along;
        logUp = up;

        // Only fails when the blade is aimed almost straight into the screen,
        // where there is no on-screen direction to take.
        if (sqrtf(along * along + up * up) > m_SlashBladeMin)
        {
            float local = atan2f(up, along) + m_SlashRollTune;
            logLocal = local;

            // Facing left is a mirror, and for this crescent - symmetric
            // about its own horizontal axis - that mirror is (PI - angle).
            rotation.z = (facing > 0.0f) ? local : (XM_PI - local);
            measured = true;
        }
    }
    else if (m_SlashAngleMode == 2)
    {
        float vx = m_WeaponVelocity.x;
        float vy = m_WeaponVelocity.y;

        if (sqrtf(vx * vx + vy * vy) > m_SlashMotionMin)
        {
            rotation.z = atan2f(vy, vx) + m_SlashRollTune;
            measured = true;
        }
    }

    if (!measured)
    {
        // Fallback: the fixed per-combo angle. The crescent is drawn already
        // bulging right, so facing right needs no base turn; facing left is
        // (PI - roll), which mirrors it, because the arc is symmetric about
        // its own horizontal axis. A negative X scale would be the obvious
        // mirror and is wrong here - back-face culling would swallow it.
        rotation.z = (facing > 0.0f) ? roll : (XM_PI - roll);
    }

    Vector3 slashScale(length, thickness, 1.0f);

    // The sweep flips with the facing too, so the blade always travels the
    // way the character is swinging rather than back into them.
    if (m_SlashLogSpawn)
    {
        char buffer[256];
        sprintf_s(buffer,
            "[SlashSpawn] combo %d  facing %s  mode %d  %s\n"
            "             blade along %+.3f  up %+.3f  len %.3f"
            "  local %+7.1f deg  -> roll %+7.1f deg\n",
            m_SpecialAttacking ? -1 : (m_AttackCombo % 3),
            (facing > 0.0f) ? "RIGHT" : "LEFT ",
            m_SlashAngleMode,
            measured ? "measured" : "FELL BACK to the table",
            logAlong, logUp, sqrtf(logAlong * logAlong + logUp * logUp),
            XMConvertToDegrees(logLocal),
            XMConvertToDegrees(rotation.z));
        OutputDebugStringA(buffer);
    }

    Manager::AddGameObj<SlashEffect>()->Play(position, rotation, slashScale,
        lifetime, 1.0f, facing * sweep);
}

void Player::StartAttack()
{
    // No damage here any more - the swing lands on its active frame, see
    // the hit window in Update(). Use() at this point hit the enemy while
    // the sword was still behind the player's back.
    m_AttackHitDone = false;
    m_SpecialAttacking = false;

    // A step into the swing. Movement is locked while attacking, so the
    // usual drag bleeds this off on its own.
    Vector3 forward = GetFoward();
    m_Velocity.x += forward.x * m_AttackLunge;

    m_AttackCombo = (m_ComboResetTimer > m_ComboWindow)
        ? 0
        : (m_AttackCombo + 1) % 3;

    m_Attacking = true;
    m_ComboResetTimer = 0.0f;

    const char* attackAnim =
        (m_AttackCombo == 0) ? "Attack1" :
        (m_AttackCombo == 1) ? "Attack2" : "Attack3";

    // Same index that picked the animation - the combo sounds different at
    // each step instead of repeating one swing three times.
    SoundEffect::Play(
        (m_AttackCombo == 0) ? SE::PlayerAttack1 :
        (m_AttackCombo == 1) ? SE::PlayerAttack2 : SE::PlayerAttack3);

    SetAnimation(attackAnim);
    m_AttackAnimLength = m_AnimationModel->GetAnimationFrameCount(attackAnim);

    m_Blend = 0.7f; // skip the idle/run→attack crossfade so the pose is
    // always the pure attack animation from frame 0,
    // matching exactly what was tuned - consistent
    // regardless of what animation played before it.
}

// Right click. Costs MP, and its opening frames parry - see IsParrying().
void Player::StartRightAttack()
{
    if (!m_Stats->TrySpendMP(m_RightAttackMPCost))
        return; // not enough MP - attack doesn't trigger

    StartCounterAttack();
}

void Player::StartCounterAttack()
{
    m_AttackHitDone = false;
    m_SpecialAttacking = true;
    m_ParryTimer = m_ParryTime; // the deflect is live from the first frame
    m_AttackQueued = false;     // a press buffered before this must not chain out of it

    Vector3 forward = GetFoward();
    m_Velocity.x += forward.x * m_AttackLunge;

    m_Attacking = true;

    SoundEffect::Play(SE::SpecialAttack);

    SetAnimation("AttackRight");
    m_AttackAnimLength = m_AnimationModel->GetAnimationFrameCount("AttackRight");

    m_Blend = 0.7f;
}