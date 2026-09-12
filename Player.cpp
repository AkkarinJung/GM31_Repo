#include "main.h"
#include "renderer.h"
#include "Player.h"
//#include "modelRenderer.h"
#include "animationModel.h"
#include "input.h"
#include "Collision.h"

#include "Manager.h"
#include "Camera.h"
#include "audio.h"

#include "Enemy.h"
#include "Tree.h"
#include "Box.h"
#include "Shadow.h"
#include "MeshField.h"

#include "Stats.h"

#include "BoneAttachPoint.h"
#include "Sword.h"

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

    //BGM
    m_JumpSE = AddGameComponent<Audio>(this);
    m_JumpSE->Load("asset\\Audio\\wan.wav");

    m_Child = Manager::AddGameObj<GameObject>();
    m_Child->SetParent(this);
    m_Child->SetPosition({ 0.0f,2.0f,0.0f });

    //ModelRenderer* childModel = m_Child->AddGameComponent<ModelRenderer>(m_Child);
    //childModel->Load("asset\\model\\Rabbit\\rabbit_1.obj");
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

    Vector3 oldPosition = m_Position;
    float dt = 1.0 / 60.0f;

    if (Input::GetKeyTrigger(VK_F4))
        DebugDumpSwing("Attack1", "mixamorig:RightHand");

    //Vector3 rot = m_Child->GetRotation();
    //rot.y += 1.0f * dt;
    //m_Child->SetRotation(rot);

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

    // Only re-aim while actually moving. atan2f(0, 0) is 0, so reading the
    // facing every frame snapped the player (and the sword parented to it)
    // round to face +Z the moment the velocity died out, and flipped it
    // 180 degrees on the frame the velocity crossed zero.
    if (fabsf(m_Velocity.x) > 0.01f || fabsf(m_Velocity.z) > 0.01f)
        m_Rotation.y = atan2f(m_Velocity.x, m_Velocity.z);

    if (Input::GetKeyTrigger(VK_SPACE))
    {
        m_Velocity.y += m_JumpPower;

        //m_Scale.y = 2.0f;
        //m_Scale.x = 0.5f;
        //m_Scale.z = 0.5f;

        m_JumpSE->Play();
    }

    //return scale to original
    //m_Scale.x += (1.0f - m_Scale.x) * 0.1f;
    //m_Scale.y += (1.0f - m_Scale.y) * 0.1f;
    //m_Scale.z += (1.0f - m_Scale.z) * 0.1f;

    m_Velocity.y += -98.0f * dt;

    m_Velocity.x += -m_Velocity.x * 5.0f * dt;
    m_Velocity.z += -m_Velocity.z * 5.0f * dt;

    m_Position += m_Velocity * dt;

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

    auto boxes = Manager::GetGameObjs<Box>();
    for (auto box : boxes)
    {
        Vector3 boxPosition = box->GetPosition();
        Vector3 boxScale = box->GetScale();

        if (boxPosition.x - boxScale.x < m_Position.x &&
            m_Position.x < boxPosition.x + boxScale.x &&
            boxPosition.z - boxScale.z < m_Position.z &&
            m_Position.z < boxPosition.z + boxScale.z)
        {
            if (boxPosition.y + boxScale.y < m_Position.y &&
                m_Position.y < boxPosition.y + boxScale.y * 2.0f)
            {
                //上面に衝突 top of the box
                m_Position.y = boxPosition.y + boxScale.y * 2.0f;
                m_Velocity.y = 0.0f;
                m_Ground = true;
            }
            else if (boxPosition.y - boxScale.y < m_Position.y &&
                m_Position.y < boxPosition.y + boxScale.y)
            {
                //側面に衝突
                m_Position.x = oldPosition.x;
                m_Position.z = oldPosition.z;
                m_Velocity.x = 0.0f;
                m_Velocity.z = 0.0f;
            }
        }
    }

    auto enemies = Manager::GetGameObjs<Enemy>();

    for (auto enemy : enemies)
    {
        const float playerRadius = 0.7f;
        const float enemyRadius = 0.7f;

        Vector3 dir = m_Position - enemy->GetPosition();
        float length = dir.lenght();

        if (Collision::SphereVsSphere(m_Position, playerRadius, enemy->GetPosition(), enemyRadius))
        {
            if (length > 0.0f)
            {
                float overlap = (playerRadius + enemyRadius) - length;
                dir /= length;
                m_Position += dir * overlap;
            }

            break;
        }
    }

    //if (!oldGround && m_Ground)
    //{
    //    m_Scale.y = 0.5f;
    //    m_Scale.x = 2.0f;
    //    m_Scale.z = 2.0f;
    //}

    if (Input::GetKeyTrigger(VK_LBUTTON))
    {
        if (m_Attacking)
            m_AttackQueued = true;   // current swing is still playing - buffer this press
        else
            StartAttack();
    }

    if (Input::GetKeyTrigger(VK_RBUTTON) && !m_Attacking)
    {
        StartRightAttack();
    }

    // Attacks take priority and run to completion; once done (or if not
    // attacking), fall back to Jump (while airborne) or Run/Idle.
    if (m_Attacking && m_NextAnimationFrame >= m_AttackAnimLength)
    {
        m_Attacking = false;

        if (m_AttackQueued)
        {
            m_AttackQueued = false;
            StartAttack();
        }
    }

    if (!m_Attacking)
    {
        m_ComboResetTimer += dt;
    }

    if (m_Ground)
    {
        //m_MoveAnimation += VectorMag(m_Velocity) * dt;
        //m_Scale.y += sinf(m_MoveAnimation * 3.0f) * 0.03f;
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

    if (!m_FreezeAnimation)
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

void Player::StartAttack()
{
    m_Weapon->Use(this);

    m_AttackCombo = (m_ComboResetTimer > m_ComboWindow)
        ? 0
        : (m_AttackCombo + 1) % 3;

    m_Attacking = true;
    m_ComboResetTimer = 0.0f;

    const char* attackAnim =
        (m_AttackCombo == 0) ? "Attack1" :
        (m_AttackCombo == 1) ? "Attack2" : "Attack3";

    SetAnimation(attackAnim);
    m_AttackAnimLength = m_AnimationModel->GetAnimationFrameCount(attackAnim);

    m_Blend = 0.7f; // skip the idle/run→attack crossfade so the pose is
    // always the pure attack animation from frame 0,
    // matching exactly what was tuned - consistent
    // regardless of what animation played before it.
}

void Player::StartRightAttack()
{
    if (!m_Stats->TrySpendMP(m_RightAttackMPCost))
        return; // not enough MP - attack doesn't trigger

    m_Weapon->Use(this);

    m_Attacking = true;

    SetAnimation("AttackRight");
    m_AttackAnimLength = m_AnimationModel->GetAnimationFrameCount("AttackRight");

    m_Blend = 0.7f;
}