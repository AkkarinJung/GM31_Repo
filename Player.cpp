#include "main.h"
#include "renderer.h"
#include "Player.h"
//#include "modelRenderer.h"
#include "animationModel.h"
#include "input.h"
#include "Collision.h"
#include "Game.h"

#include "Manager.h"
#include "Camera.h"
#include "SoundEffect.h"

#include "Tree.h"
#include "Shadow.h"
#include "MeshField.h"

#include "Stats.h"
#include "PotionBag.h"

#include "BoneAttachPoint.h"
#include "Sword.h"
#include "Result.h"
#include "Particle.h"
#include "Fade.h"

#include "SlashArc.h"
#include "SwordTrail.h"
#include "ImpactEffect.h"

// How far through the current swing, 0..1. Anything that is not mid-swing
// reads as finished, so callers do not have to special-case it.
float Player::AttackProgress() const
{
    if (!m_Attacking || m_AttackAnimLength <= 0)
        return 1.0f;

    // Relative to the slice being played, not to the whole clip - the clip
    // may start partway in and always ends early, and every phase boundary
    // below is expressed in this space.
    float t = (m_AttackFrame - m_AttackClipFirst) / m_AttackClipSpan;
    return (t < 0.0f) ? 0.0f : (t > 1.0f ? 1.0f : t);
}

// Which swing is landing, for the three things that have to scale together:
// how hard it hits, how long the impact freeze holds it, and how far the
// camera kicks. Split out so the combo step is read in one place rather than
// indexed into three tables at three different call sites.
//
// m_AttackCombo is always 0..2 (StartAttack keeps it there with % 3), but
// these index arrays, so they clamp rather than trust that from a distance.
static int ComboIndex(int Combo)
{
    if (Combo < 0) return 0;
    if (Combo > 2) return 2;
    return Combo;
}

float Player::SwingDamageScale() const
{
    return m_SpecialAttacking ? m_SpecialDamageScale
                              : m_ComboDamageScale[ComboIndex(m_AttackCombo)];
}

int Player::SwingHitStop() const
{
    return m_SpecialAttacking ? m_SpecialHitStop
                              : m_ComboHitStop[ComboIndex(m_AttackCombo)];
}

float Player::SwingShake() const
{
    return m_SpecialAttacking ? m_SpecialShake
                              : m_ComboShake[ComboIndex(m_AttackCombo)];
}

float Player::SwingImpactScale() const
{
    return m_SpecialAttacking ? m_SpecialImpactScale
                              : m_ImpactScale[ComboIndex(m_AttackCombo)];
}

// Turns "the swing should take this many seconds" into "advance the key
// index by this much per tick", for whichever clip just started.
//
// Called once per swing rather than per frame: the clip length cannot change
// mid-swing, so there is nothing here worth recomputing 60 times a second.
// Where the real swing sits inside each attack clip.
//
// Start/End are fractions of the CLIP. HitPoint/HitEnd are fractions of the
// SLICE those cut out - so they stay correct when a slice is re-cut.
//
// These are measured, not guessed. Press F4 and read DebugMeasureSwing's
// output: it walks the clip, tracks how fast the weapon hand moves, and
// reports the contact key and the shoulders of the swing.
//
// Attack1 is what the measurement actually found, and it is worth reading as
// a warning about the defaults it replaced:
//
//   137 keys. The hand accelerates from key ~34, peaks at key 49 - that is
//   the blade arriving - and is spent by key 59. Keys 60-136, a clear 56% of
//   the clip, are the character drifting its arm back to neutral.
//
//   Playing the clip from key 0 to key 109 (the old global 0.00-0.80) opened
//   the hitbox at key 38, eleven keys and about 0.18s BEFORE the sword got
//   there, and then spent fifty keys of recovery budget on the drift home.
//   Slicing 34..67 instead puts contact on the key the blade actually lands
//   on and leaves every phase running at 1.0-1.15 keys per tick, which for
//   an engine that does not interpolate between keys is the whole ballgame.
//
// A clip with no row here falls back to the old whole-clip behaviour: safe,
// just not tuned. Measure it and add a row.
struct SwingSlice
{
    const char* Name;
    float Start;    // fraction of the clip the slice begins at
    float End;      // fraction of the clip it ends at
    float HitPoint; // fraction of the SLICE the blade lands on
    float HitEnd;   // fraction of the SLICE the hitbox closes at
};

static const SwingSlice SWING_SLICES[] =
{
    // All four measured. Every slice is 33 keys wide because the time budgets
    // fix it: ~15 keys of windup, 6 of strike, 12 of follow through is what
    // 0.22s/0.10s/0.18s buys at a bit over one key per tick. So the whole
    // table is really one number per clip - where the blade lands - with the
    // slice hung around it. HitPoint and HitEnd come out identical for that
    // reason; they are not copy-paste.
    //
    // name           start     end   hit   hitEnd        contact  clip
    { "Attack1",      0.248f, 0.489f, 0.455f, 0.636f }, // key  49  of 137
    { "Attack2",      0.421f, 0.669f, 0.455f, 0.636f }, // key  71  of 133
    { "Attack3",      0.220f, 0.582f, 0.455f, 0.636f }, // key  35  of  91
    { "AttackRight",  0.337f, 0.663f, 0.455f, 0.636f }, // key  49  of 101
};

void Player::SetupSwingClock(const char* AnimationName)
{
    // Defaults are the untuned whole-clip behaviour, so a clip that is not in
    // the table still swings - it just swings the old way.
    m_AttackClipStart = 0.00f;
    m_AttackClipEnd = 0.80f;
    m_AttackHitPoint = 0.35f;
    m_AttackHitEnd = 0.55f;

    if (AnimationName != nullptr)
    {
        for (const SwingSlice& slice : SWING_SLICES)
        {
            if (strcmp(slice.Name, AnimationName) == 0)
            {
                m_AttackClipStart = slice.Start;
                m_AttackClipEnd = slice.End;
                m_AttackHitPoint = slice.HitPoint;
                m_AttackHitEnd = slice.HitEnd;
                break;
            }
        }
    }

    if (m_AttackAnimLength <= 0)
    {
        m_AttackClipFirst = 0.0f;
        m_AttackClipLast = 1.0f;
        m_AttackClipSpan = 1.0f;
        return;
    }

    m_AttackClipFirst = (float)m_AttackAnimLength * m_AttackClipStart;
    m_AttackClipLast = (float)m_AttackAnimLength * m_AttackClipEnd;

    m_AttackClipSpan = m_AttackClipLast - m_AttackClipFirst;
    if (m_AttackClipSpan < 1.0f)
    {
        // Degenerate slice (a clip of one or two keys, or start/end set
        // crossed over) - fall back to the whole clip rather than dividing
        // by ~0 and launching the frame counter into the wrap in Update().
        m_AttackClipFirst = 0.0f;
        m_AttackClipLast = (float)m_AttackAnimLength;
        m_AttackClipSpan = (float)m_AttackAnimLength;
    }

    // Keys to cover in each phase / ticks that phase is allowed to take.
    const float TICKS = 60.0f;
    float windupKeys = m_AttackClipSpan * m_AttackHitPoint;
    float strikeKeys = m_AttackClipSpan * (m_AttackHitEnd - m_AttackHitPoint);
    float recoverKeys = m_AttackClipSpan * (1.0f - m_AttackHitEnd);

    m_AttackRateWindup = windupKeys / (m_SwingWindupTime * TICKS);
    m_AttackRateStrike = strikeKeys / (m_SwingActiveTime * TICKS);
    m_AttackRateRecover = recoverKeys / (m_SwingRecoverTime * TICKS);

    // A rate of 0 would stall the swing forever - it never reaches the end
    // test and m_Attacking never clears, which locks movement permanently.
    const float MIN_RATE = 0.05f;
    if (m_AttackRateWindup < MIN_RATE)  m_AttackRateWindup = MIN_RATE;
    if (m_AttackRateStrike < MIN_RATE)  m_AttackRateStrike = MIN_RATE;
    if (m_AttackRateRecover < MIN_RATE) m_AttackRateRecover = MIN_RATE;

    // Ceiling, for clips with no measured row yet.
    //
    // Holding the timings fixed means a phase that has to cover a lot of keys
    // covers them fast, and past about 2 keys a tick this engine stops
    // reading as a fast animation and starts reading as a broken one - there
    // is no interpolation between keys, so a rate of 5 does not blur, it
    // shows every fifth pose. An unmeasured clip keeps its whole 130-180 key
    // body and lands squarely in that range.
    //
    // A clipped phase overruns its budget rather than strobing, so those
    // swings are slower than the numbers above ask for. That is the right
    // trade: an untuned swing that plays smoothly and late beats one that
    // hits on time and flickers. Measure the clip and the ceiling stops
    // mattering - every phase of a measured slice sits near 1.1.
    const float MAX_RATE = 2.0f;
    if (m_AttackRateWindup > MAX_RATE)  m_AttackRateWindup = MAX_RATE;
    if (m_AttackRateStrike > MAX_RATE)  m_AttackRateStrike = MAX_RATE;
    if (m_AttackRateRecover > MAX_RATE) m_AttackRateRecover = MAX_RATE;
}

void Player::BeginDeath()
{
    m_Dead = true;
    m_DeathTimer = 0.0f;
    m_DeathFrame = 0.0f;

    // Drop everything mid-action. A swing left running would keep its
    // hitbox, and the special would leave the parry window open on a
    // corpse.
    m_Attacking = false;
    m_SpecialAttacking = false;
    m_ParryTimer = 0.0f;
    m_HitStopFrames = 0;

    // The ribbon goes with them. Update() returns at the death branch above
    // this from now on, so nothing would ever call End() on it - and a trail
    // left emitting would keep sampling a blade that is falling over.
    if (m_SwordTrail != nullptr)
    {
        m_SwordTrail->SetFrozen(false);
        m_SwordTrail->Clear();
    }

    m_TrailRunning = false;

    m_Velocity.x = 0.0f;
    m_Velocity.z = 0.0f;

    SetAnimation("Death");

    // Played from here rather than from anything attached to the player: the
    // scene is torn down at the end of this, and a voice owned by an object
    // in it would be cut off with it. The same reason Enemy plays its death
    // sound from Enemy.
    SoundEffect::Play(SE::PlayerDeath);
}

void Player::UpdateDeath()
{
    const float dt = 1.0f / 60.0f;

    m_DeathTimer += dt;

    // Still falls. Dying in mid air and hanging there would read as the game
    // having frozen rather than as the player having died.
    m_Velocity.y += -98.0f * dt;

    std::vector<AABB> solids = Collision::GatherSolids();

    bool landed = false;
    if (Collision::MoveY(m_Position, m_BodyHalfSize, m_Velocity.y * dt, solids, landed))
        m_Velocity.y = 0.0f;

    MeshField* meshField = Manager::GetGameObj<MeshField>();

    if (meshField != nullptr)
    {
        float height = meshField->GetHeight(m_Position);

        if (m_Position.y < height)
        {
            m_Position.y = height;
            m_Velocity.y = 0.0f;
        }
    }

    // Play the clip once and HOLD the last key. AnimationModel::Update takes
    // frame % numKeys, so letting the counter run past the end would loop the
    // death back to standing.
    int length = m_AnimationModel->GetAnimationFrameCount("Death");

    m_DeathFrame += m_DeathAnimRate;

    if (length > 0 && m_DeathFrame > (float)(length - 1))
        m_DeathFrame = (float)(length - 1);

    m_NextAnimationFrame = (int)m_DeathFrame;
    m_AnimationFrame = m_NextAnimationFrame;
    m_Blend = 1.0f;

    m_AnimationModel->Update("Death", m_AnimationFrame, "Death", m_NextAnimationFrame, 1.0f);

    // Then the run is over. Asked for once - ChangeScene ignores a second
    // call while one is pending, but the guard says so out loud.
    if (!m_ResultRequested && m_DeathTimer >= m_DeathHold)
    {
        m_ResultRequested = true;
        Manager::ChangeScene<Result>(2.0f);
        Fade::OutBefore(2.0f);
    }

    GameObject::Update();
}

bool Player::MovementLocked() const
{
    return m_Attacking && AttackProgress() < m_AttackMoveUnlock;
}

void Player::Init()
{
    m_Layer = 1;
    m_Position = { 0.0f, 0.0f, 0.0f };
    m_Scale = { 0.01f, 0.01f, 0.01f };

    // Face down +X, the way the stages run.
    //
    // This has to be set, not left at zero. The facing is written as
    // atan2f(m_Velocity.x, m_Velocity.z) further down, and z is always zero
    // in a 2.5D game - so the only two values that code can ever produce are
    // +PI/2 (right) and -PI/2 (left). A default yaw of 0 is neither of them:
    // it points the player straight into the screen, which is why he started
    // every stage facing the camera until the first key was pressed.
    m_Rotation.y = atan2f(1.0f, 0.0f);

    //ModelRenderer* m_ModelRenderer = AddGameComponent<ModelRenderer>(this);
    //m_ModelRenderer->Load("asset\\model\\player.obj");
    m_AnimationModel = AddGameComponent<AnimationModel>(this);
    m_AnimationModel->Load("asset\\model\\Player_Movement\\Idle_model.fbx");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Movement\\Idle_model.fbx", "Idle");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Movement\\Run.fbx", "Run");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Movement\\Jump.fbx", "Jump");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Movement\\Death.fbx", "Death");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Attack\\Attack_1.fbx", "Attack1");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Attack\\Attack_2.fbx", "Attack2");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Attack\\Attack_3.fbx", "Attack3");
    m_AnimationModel->LoadAnimation("asset\\model\\Player_Attack\\Attack_Right.fbx", "AttackRight");
    m_AnimationModel->DebugPrintBoneNames();

    // Playback is one key per game frame with no reference to the rate the
    // clips were authored at - see AnimationModel::DebugPrintAnimationInfo.
    // This says, in the output window, whether that is a problem here.
    m_AnimationModel->DebugPrintAnimationInfo();

    m_AnimationName = "Idle";
    m_NextAnimationName = "Idle";

    m_WeaponSocket = AddGameComponent<BoneAttachPoint>(this);
    m_WeaponSocket->SetBone(m_AnimationModel, "mixamorig:RightHand");
    m_WeaponSocket->SetLocalTransform(m_WeaponOffsetPos, m_WeaponOffsetRot, { 1.0f, 1.0f, 1.0f });
    m_Weapon = Manager::AddGameObj<Sword>();
    m_WeaponSocket->Attach(m_Weapon);

    // The blade ribbon. Built once, here, and fed by UpdateSwingTrail - a
    // swing never spawns one. It follows the weapon through GetMatrx(), so
    // the hand socket and the player's facing are already in it and nothing
    // here has to know which way he is pointing.
    m_SwordTrail = Manager::AddGameObj<SwordTrail>();
    m_SwordTrail->SetBlade(m_Weapon, m_TrailBaseReach, m_TrailTipReach);

    // Pull the shared VFX resources in now rather than on the first swing of
    // the stage - a mesh built or a texture read mid-swing is a visible
    // stall. Both calls are idempotent.
    SlashArc::LoadShared();
    ImpactEffect::LoadShared();

    // シェーダー読込
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\litTextureVS.cso");

    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\litTexturePS.cso");

    m_Shadow = Manager::AddGameObj<Shadow>();
    m_Shadow->SetScale({ 1.5f ,1.5f ,1.5f });

    m_Stats = AddGameComponent<Stats>(this);
    m_Stats->SetMaxHP(100);
    m_Stats->SetMaxMP(50); // explicit rather than relying on the Stats default
    m_Stats->SetAttack(5); // base unarmed attack - weapons add on top of this
    m_Stats->SetDefense(2);
}

void Player::Uninit()
{
    // Manager owns every GameObject and deletes them all on a scene change,
    // so this is not a delete - it is just making sure nothing in a later
    // frame can reach a trail that has already been torn down.
    m_SwordTrail = nullptr;

    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();

    GameObject::Uninit();
}

void Player::Update()
{
    // Death first, before a single line of input is read. Everything below
    // this - the swing, the potions, the movement - belongs to a player who
    // is still alive.
    if (!m_Dead && m_Stats != nullptr && m_Stats->IsDead())
        BeginDeath();

    if (m_Dead)
    {
        UpdateDeath();
        return;
    }

    // 1/2/3 jump to the start / middle / end of the current swing, which is
    // how you check the grip at the extremes of the animation (F2 freezes).
    // These drive the swing clock, not the frame it derives - setting the
    // frame alone would be overwritten on the very next tick.
    if (Input::GetKeyTrigger('1') && m_Attacking) { m_AttackFrame = m_AttackClipFirst; m_NextAnimationFrame = (int)m_AttackFrame; m_Blend = 1.0f; }
    if (Input::GetKeyTrigger('2') && m_Attacking) { m_AttackFrame = m_AttackClipFirst + m_AttackClipSpan * 0.5f; m_NextAnimationFrame = (int)m_AttackFrame; m_Blend = 1.0f; }
    if (Input::GetKeyTrigger('3') && m_Attacking) { m_AttackFrame = m_AttackClipLast - 1.0f; m_NextAnimationFrame = (int)m_AttackFrame; m_Blend = 1.0f; }

    // Drink from a potion slot. 1 and 2 are the debug scrub above while a
    // swing is playing, so this takes the other half of that condition
    // rather than a key of its own: the two can never both fire on one
    // press, whatever else changes around them. It also means a swing
    // cannot be cancelled into a heal, which is the right way round - you
    // commit to the attack.
    if (!m_Attacking)
    {
        for (int slot = 0; slot < PotionBag::SlotCount; slot++)
        {
            if (Input::GetKeyTrigger((BYTE)('1' + slot)))
                PotionBag::Use(slot, this);
        }
    }

    if (Input::GetKeyTrigger(VK_F2))
        m_FreezeAnimation = !m_FreezeAnimation;

    if (m_FreezeAnimation && Input::GetKeyTrigger(VK_F3))
    {
        m_AnimationFrame++;
        m_NextAnimationFrame++;
    }

    float dt = 1.0 / 60.0f;

    if (Input::GetKeyTrigger(VK_F4))
    {
        // Every attack clip, not just the first - the slice has to be set per
        // clip and they are all different lengths.
        DebugMeasureSwing("Attack1", "mixamorig:RightHand");
        DebugMeasureSwing("Attack2", "mixamorig:RightHand");
        DebugMeasureSwing("Attack3", "mixamorig:RightHand");
        DebugMeasureSwing("AttackRight", "mixamorig:RightHand");
    }

    bool oldGround = m_Ground;
    m_Ground = false;

    // 2.5D side-scroll movement: only left/right along world X - no
    // depth/forward-back input, since the camera no longer rotates to
    // face any other direction. Locked out while attacking.
    //
    // The direction itself is read whether or not movement is locked: the
    // turn window further down needs it during the windup, which is exactly
    // when MovementLocked() is true.
    float steerX = 0.0f;
    if (Input::GetKeyPress('D')) steerX += 1.0f;
    if (Input::GetKeyPress('A')) steerX -= 1.0f;

    bool move = false;

    if (!MovementLocked() && steerX != 0.0f)
    {
        // Full speed out of a swing, reduced while the recovery plays out -
        // enough to reposition, not enough to make the recovery free.
        float speed = m_Attacking ? m_MoveSpeed * m_AttackMoveScale : m_MoveSpeed;

        m_Velocity.x += steerX * speed * dt;
        move = true;
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

    // Only re-aim while actually moving. atan2f(0, 0) is 0, so reading the
    // facing every frame snapped the player (and the sword parented to it)
    // round to face +Z the moment the velocity died out, and flipped it
    // 180 degrees on the frame the velocity crossed zero.
    // Not while attacking either: the lunge and the recoil are velocity too,
    // and reading the facing off them spun the player away from the enemy
    // mid-swing.
    // Gated on "is the player steering", not on "is the velocity non-zero".
    // The lunge and the recoil are velocity too, and reading the facing off
    // them spun the player away from the enemy mid-swing - but once movement
    // is back during recovery, a held direction should turn him.
    bool steering = !MovementLocked() && move; // recovery, or free to move
    bool coasting = !m_Attacking;              // normal movement, key released

    if ((steering || coasting) &&
        (fabsf(m_Velocity.x) > 0.01f || fabsf(m_Velocity.z) > 0.01f))
        m_Rotation.y = atan2f(m_Velocity.x, m_Velocity.z);

    // A swing may still be turned round until the blade comes out.
    //
    // This reads the INPUT, not the velocity, which is what makes it safe to
    // do mid-swing at all: the block above deliberately refuses to face off
    // the velocity while attacking, because the lunge and the recoil are
    // velocity too and reading the facing from them spun the player away from
    // the enemy. A held direction key is unambiguous - it is the player
    // saying which way they want to swing - so it does not have that problem.
    //
    // After m_AttackTurnWindow the direction is committed. Being able to
    // rotate a swing that is already landing would make the arc unreadable
    // and let a single press cover both sides of the player.
    if (m_Attacking && steerX != 0.0f && AttackProgress() < m_AttackTurnWindow)
        m_Rotation.y = atan2f(steerX, 0.0f);

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

        // Dust off the floor, from here rather than from Particle watching
        // the key itself: this branch is the only place a jump actually
        // happens. Reading SPACE over there also fired on presses the jump
        // refused - in mid air, most obviously - so the puff appeared with
        // the player nowhere near the ground.
        Particle* particle = Manager::GetGameObj<Particle>();

        if (particle != nullptr)
            particle->JumpDust(m_Position);
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

    // And the map edge gets the last word after that.
    //
    // This is a backstop, not the fix: MoveX sweeps now and the edge hedges
    // carry a minimum-thickness collider, so nothing should reach here out of
    // bounds. But every other guard is collision code that can be defeated by
    // a bad frame, and being outside the map is unrecoverable - there is no
    // floor, no way back, and the camera has already stopped. A hard clamp
    // costs two comparisons and makes it impossible rather than unlikely.
    if (m_Position.x < Game::MapLeft + m_BodyHalfSize.x)
    {
        m_Position.x = Game::MapLeft + m_BodyHalfSize.x;
        if (m_Velocity.x < 0.0f) m_Velocity.x = 0.0f;
    }
    else if (m_Position.x > Game::MapRight - m_BodyHalfSize.x)
    {
        m_Position.x = Game::MapRight - m_BodyHalfSize.x;
        if (m_Velocity.x > 0.0f) m_Velocity.x = 0.0f;
    }


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
    else
    {
        // Full: drop the part-point. Keeping it meant the first point after
        // the next spend arrived early by however much had been banked while
        // the bar sat full, which made the regen rate look inconsistent.
        m_MPRegenCarry = 0.0f;
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
    // button went down - and it stays dangerous for a few frames rather than
    // for exactly one.
    if (m_Attacking && m_AttackAnimLength > 0)
    {
        int windowOpen = (int)(m_AttackClipFirst + m_AttackClipSpan * m_AttackHitPoint);

        // Latched on >=, exactly as the single frame version was, so nothing
        // that moves the frame counter in jumps (hit stop, the 1/2/3 debug
        // keys) can step over the window and produce a swing with no hitbox
        // at all. Its length is then counted in frames of its own.
        if (!m_AttackHitDone && m_NextAnimationFrame >= windowOpen)
        {
            m_AttackHitDone = true;

            int windowClose = (int)(m_AttackClipFirst + m_AttackClipSpan * m_AttackHitEnd);
            m_AttackHitFrames = windowClose - windowOpen + 1;
            if (m_AttackHitFrames < 1)
                m_AttackHitFrames = 1;

            // The cut, spawned here rather than in StartAttack: at the start
            // of a swing the sword is still behind the player's back, which
            // is why the damage waits for m_AttackHitPoint too.
            //
            // Visual and hitbox are separate on purpose. The cut shows on
            // every swing, hit or miss, and may reach further than the sword
            // actually does - changing how it looks can never change what it
            // damages.
            SpawnSlashArc();

            // Which step of the combo this is decides what it is worth. Set
            // before BeginSwing so the very first frame of the active window
            // already carries it - Use() runs on every frame of that window.
            m_Weapon->SetSwingMultiplier(SwingDamageScale());
            m_Weapon->BeginSwing();
        }

        // Not while the impact freeze is holding: the animation is paused for
        // those frames, so spending the window there would slide the hitbox
        // out from under the pose it belongs to.
        if (m_AttackHitFrames > 0 && m_HitStopFrames <= 0)
        {
            m_AttackHitFrames--;

            // Use() reports true only for an enemy this swing has not already
            // damaged, so the impact feedback fires once per enemy however
            // long it stands in the arc.
            if (m_Weapon->Use(this))
            {
                // Connected: hold the frame for a moment and kick the camera.
                // Both scale with the combo step, so the finisher stops the
                // frame and shoves the camera harder than the opener does.
                m_HitStopFrames = SwingHitStop();
                SoundEffect::Play(SE::SwordHit);

                // And the burst, on each target the weapon says it just
                // damaged. This is the ONLY place an impact is spawned, and
                // it is inside the branch that asked the weapon whether it
                // hit - a miss reaches none of it and shows the ribbon and
                // the cut alone.
                SpawnImpacts();

                Camera* camera = Manager::GetGameObj<Camera>();
                if (camera != nullptr)
                    camera->Shake(GetFoward() * SwingShake());
            }
        }
    }

    // Against the float clock and the slice end, not the key index and the
    // clip length: the slice stops short of the clip, and the key index is
    // wrapped by AnimationModel::Update() so it can never report the overrun.
    if (m_Attacking && m_AttackFrame >= m_AttackClipLast)
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
                m_AttackFrame >= m_AttackClipFirst + m_AttackClipSpan * m_ComboCancelPoint);

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
        if (m_Attacking && m_AttackAnimLength > 0)
        {
            // The swing accelerates through its own phases. A flat one key
            // per frame is what made every part of the swing take the same
            // time, so there was no moment of impact in it.
            float t = AttackProgress();

            float rate = (t < m_AttackHitPoint) ? m_AttackRateWindup
                       : (t < m_AttackHitEnd)   ? m_AttackRateStrike
                                                : m_AttackRateRecover;

            m_AttackFrame += rate;
            m_NextAnimationFrame = (int)m_AttackFrame;

            // AnimationModel::Update() does f = Frame % numKeys, so a key
            // index that reaches the clip length wraps to 0 - the neutral
            // pose - for the one tick between the clock passing the slice end
            // and the end test below clearing m_Attacking. That is a visible
            // snap to T-pose-ish at the end of every swing. Only reachable
            // with a slice that runs to the last key (ClipEnd 1.0, or the
            // degenerate fallback in SetupSwingClock), but it costs one line
            // to make unreachable.
            if (m_NextAnimationFrame >= m_AttackAnimLength)
                m_NextAnimationFrame = m_AttackAnimLength - 1;
        }
        else
        {
            m_NextAnimationFrame++;
        }

        m_AnimationFrame++;
        m_Blend += 0.1f;
        if (m_Blend > 1.0f)
            m_Blend = 1.0f;
    }

    m_AnimationModel->Update(m_AnimationName.c_str(), m_AnimationFrame, m_NextAnimationName.c_str(), m_NextAnimationFrame, m_Blend);

    GameObject::Update();

    // LAST, and after GameObject::Update() rather than before it. The weapon
    // socket is a component, so the blade's matrix is only this frame's once
    // the components above have run - sampling any earlier would draw the
    // ribbon one frame behind the sword, which on the fastest part of a swing
    // is a visible gap between the blade and its own trail.
    UpdateSwingTrail();
}

void Player::Draw()
{
    // The shadow is placed here, not in Update. Update does not run while the
    // game is paused, and the reward card screen pauses at the end of
    // Game::Init - before any Update at all - which left every shadow in the
    // map sitting on the world origin for the whole card screen. Draw always
    // runs. Shadow is layer 2 and the player layer 1, so it is already
    // positioned by the time it draws itself.
    Vector3 shadowPos = m_Position;
    shadowPos.y = 0.01f;
    m_Shadow->SetPosition(shadowPos);

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

// The clip slice is the one number in the swing tuning that cannot be
// reasoned about from outside - it depends entirely on how the artist
// exported the motion. So measure it rather than guess it.
//
// Method: step the clip a key at a time, ask where the weapon hand is, and
// difference it. The hand's speed profile of a sword swing is a single sharp
// spike (the strike) sitting on a low plateau (settling in and out). The
// spike's peak is the contact frame and its shoulders are where the usable
// motion begins and ends. Everything outside them is the character walking
// its arms back to neutral, which the blend into Idle/Run already covers.
void Player::DebugMeasureSwing(const char* AnimationName, const char* BoneName)
{
    const int MAX_KEYS = 512;
    int frameCount = m_AnimationModel->GetAnimationFrameCount(AnimationName);

    char buffer[256];
    if (frameCount <= 2 || frameCount > MAX_KEYS)
    {
        sprintf_s(buffer, "--- %s: %d keys, nothing to measure ---\n",
            AnimationName, frameCount);
        OutputDebugStringA(buffer);
        return;
    }

    static float speed[MAX_KEYS];
    XMFLOAT3 prev{ 0.0f, 0.0f, 0.0f };
    bool havePrev = false;
    float peakSpeed = 0.0f;
    int peakFrame = 0;

    for (int f = 0; f < frameCount; f++)
    {
        m_AnimationModel->Update(AnimationName, f, AnimationName, f, 1.0f);

        XMMATRIX boneMatrix;
        if (!m_AnimationModel->GetBoneMatrix(BoneName, &boneMatrix))
        {
            speed[f] = 0.0f;
            continue;
        }

        XMVECTOR scale, rotQuat, translation;
        XMMatrixDecompose(&scale, &rotQuat, &translation, boneMatrix);

        XMFLOAT3 pos;
        XMStoreFloat3(&pos, translation);

        if (havePrev)
        {
            float dx = pos.x - prev.x;
            float dy = pos.y - prev.y;
            float dz = pos.z - prev.z;
            speed[f] = sqrtf(dx * dx + dy * dy + dz * dz);
        }
        else
        {
            speed[f] = 0.0f; // no previous key to difference against
        }

        if (speed[f] > peakSpeed)
        {
            peakSpeed = speed[f];
            peakFrame = f;
        }

        prev = pos;
        havePrev = true;
    }

    sprintf_s(buffer, "--- %s (%d keys) ---\n", AnimationName, frameCount);
    OutputDebugStringA(buffer);

    if (peakSpeed <= 0.0f)
    {
        sprintf_s(buffer, "  hand '%s' never moves - wrong bone name?\n", BoneName);
        OutputDebugStringA(buffer);
        return;
    }

    // Sparkline, so the shape can be eyeballed instead of trusted. One
    // column per 1/48th of the clip, height 0-9 relative to the peak.
    const int COLS = 48;
    char spark[COLS + 1];
    for (int c = 0; c < COLS; c++)
    {
        int from = (int)((float)c * frameCount / COLS);
        int to = (int)((float)(c + 1) * frameCount / COLS);
        if (to <= from) to = from + 1;
        if (to > frameCount) to = frameCount;

        float peak = 0.0f;
        for (int f = from; f < to; f++)
            if (speed[f] > peak) peak = speed[f];

        int h = (int)(peak / peakSpeed * 9.0f);
        spark[c] = (char)('0' + (h < 0 ? 0 : (h > 9 ? 9 : h)));
    }
    spark[COLS] = '\0';
    sprintf_s(buffer, "  hand speed |%s|\n", spark);
    OutputDebugStringA(buffer);

    // Shoulders of the spike: walk out from the peak to where the hand drops
    // below a fraction of its fastest. Reported for context only - it says
    // how much of the clip is genuinely moving, which is worth seeing, but it
    // is NOT what the slice is taken from. Walking the shoulders on Attack1
    // gives 61 keys, and 61 keys through the time budgets is 2.03x, which
    // strobes. How much motion exists and how much of it there is time to
    // play are different questions.
    const float SHOULDER = 0.15f;
    float threshold = peakSpeed * SHOULDER;

    int first = peakFrame;
    while (first > 0 && speed[first] > threshold)
        first--;

    int last = peakFrame;
    while (last < frameCount - 1 && speed[last] > threshold)
        last++;

    sprintf_s(buffer, "  contact at key %d (%.1f%% in), moving keys %d..%d = %d of %d\n",
        peakFrame, 100.0f * peakFrame / frameCount, first, last,
        last + 1 - first, frameCount);
    OutputDebugStringA(buffer);

    // The slice is not the moving part - it is what the time budgets can
    // afford, hung around the contact key.
    //
    // Each phase gets however many keys it can play at about one per tick,
    // and they are laid either side of the key the blade lands on. Asking for
    // the moving part instead is what produced a 61 key slice for Attack1 and
    // a 2.03x rate; asking what fits produces 33 keys and 1.14x, with contact
    // on the same key either way.
    //
    // A consequence worth knowing: every clip comes out the same width, so
    // every swing in the game plays at the same rate and lands at the same
    // moment. The only thing that differs between clips is where the window
    // sits inside them.
    const float TARGET_RATE = 1.15f;
    int windupKeys = (int)(m_SwingWindupTime * 60.0f * TARGET_RATE + 0.5f);
    int strikeKeys = (int)(m_SwingActiveTime * 60.0f + 0.5f);
    int recoverKeys = (int)(m_SwingRecoverTime * 60.0f * TARGET_RATE + 0.5f);

    int sliceFirst = peakFrame - windupKeys;
    int sliceLast = peakFrame + strikeKeys + recoverKeys;

    // Slide it back inside the clip rather than truncating, so the span - and
    // therefore the playback rate - survives a contact key near either end.
    if (sliceFirst < 0)
    {
        sliceLast -= sliceFirst;
        sliceFirst = 0;
    }
    if (sliceLast > frameCount - 1)
    {
        sliceFirst -= (sliceLast - (frameCount - 1));
        sliceLast = frameCount - 1;
    }
    if (sliceFirst < 0)
        sliceFirst = 0;

    float sliceSpan = (float)(sliceLast - sliceFirst);
    if (sliceSpan < 1.0f)
        sliceSpan = 1.0f;

    sprintf_s(buffer, "  slice keys %d..%d = %.0f, contact %.0f%% through it\n",
        sliceFirst, sliceLast, sliceSpan,
        100.0f * (peakFrame - sliceFirst) / sliceSpan);
    OutputDebugStringA(buffer);

    sprintf_s(buffer, "  ROW:  { \"%s\", %.3ff, %.3ff, %.3ff, %.3ff },\n",
        AnimationName,
        (float)sliceFirst / (float)frameCount,
        (float)sliceLast / (float)frameCount,
        (float)(peakFrame - sliceFirst) / sliceSpan,
        (float)(peakFrame + strikeKeys - sliceFirst) / sliceSpan);
    OutputDebugStringA(buffer);

    float rW = (sliceSpan * ((float)(peakFrame - sliceFirst) / sliceSpan))
             / (m_SwingWindupTime * 60.0f);
    float rR = (float)(sliceLast - peakFrame - strikeKeys)
             / (m_SwingRecoverTime * 60.0f);
    sprintf_s(buffer, "  -> rates %.2f / 1.00 / %.2f  (near 1.0 is smooth, over 2.0 strobes)\n",
        rW, rR);
    OutputDebugStringA(buffer);
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

// Spawns the swing's crescent. Called from the hit window, not from
// StartAttack - see the note at the call site.
void Player::SpawnSlashArc()
{
    // Which way this swing is going, taken from the player's ACTUAL facing
    // rather than assumed. The play plane is XY and the facing is a yaw about
    // Y, so the sign of the forward vector's x is the whole of it in 2.5D.
    Vector3 forward = GetFoward();

    float facing = (forward.x < 0.0f) ? -1.0f : 1.0f;

    // All three axes: along the facing, up, and through depth.
    Vector3 position = m_Position;
    position += forward * m_ArcForward;
    position.y += m_ArcHeight;
    position.z += m_ArcDepth;

    float aim;
    float scale;
    float sweep;
    float lifetime;

    // One branch per kind of attack, and the shape a new one would take.
    // Adding a heavy, an air, a dash or a charged attack later means another
    // set of numbers and another arm here - not a second effect class, and
    // nothing at all in SlashArc, SwordTrail or ImpactEffect.
    if (m_SpecialAttacking)
    {
        aim = m_ArcAimBase + m_SpecialArcAngle;
        scale = m_SpecialArcScale;
        sweep = m_SpecialArcSweep;
        lifetime = m_SpecialArcLifetime;
    }
    else
    {
        int step = ComboIndex(m_AttackCombo);

        aim = m_ArcAimBase + m_ArcAngle[step];
        scale = m_ArcScale[step];
        sweep = m_ArcSweep[step];
        lifetime = m_ArcLifetime;
    }

    // Turning around mirrors the cut through the vertical, and multiplying
    // the whole angle by the facing is exactly that mirror: the crescent's
    // belly points -X instead of +X and every per-step tilt leans the other
    // way with it. The sweep flips for the same reason, so the cut always
    // travels the way the character is swinging rather than back into them.
    float angle = facing * aim;

    Manager::AddGameObj<SlashArc>()->Play(position, angle,
        m_ArcLength * scale, m_ArcBow * scale,
        lifetime, facing * sweep);
}

// Opens, feeds and closes the blade ribbon. Called once a frame from the end
// of Update, and it is the only thing that touches m_SwordTrail during play.
void Player::UpdateSwingTrail()
{
    if (m_SwordTrail == nullptr)
        return;

    // The impact freeze holds the animation, so it has to hold the ribbon
    // too: the blade is not moving, and a trail that kept ageing through a
    // nine frame stop would fade out during the one moment it exists to sell.
    bool frozen = (m_HitStopFrames > 0);
    m_SwordTrail->SetFrozen(frozen);

    // Not swinging - which covers the swing ending, being cancelled into the
    // next one, and being interrupted by anything at all. Whatever is already
    // drawn stays and fades on its own clock, so the ribbon dies with the
    // swing instead of being cut off in mid-air.
    if (!m_Attacking)
    {
        if (m_TrailRunning)
        {
            m_SwordTrail->End();
            m_TrailRunning = false;
        }
        return;
    }

    float t = AttackProgress();

    // The facing, read the same way SpawnSlashArc reads it.
    float facing = (GetFoward().x < 0.0f) ? -1.0f : 1.0f;

    if (!m_TrailRunning)
    {
        // Still winding up. The blade is behind his back for the whole of
        // this, and a ribbon on it would draw a streak across the character
        // before the swing has started.
        if (t < m_TrailOpen)
            return;

        m_SwordTrail->SetSampleLife(m_SpecialAttacking ? m_TrailLifeSpecial
                                                       : m_TrailLife);
        m_SwordTrail->Begin();

        m_TrailRunning = true;
        m_TrailFacing = facing;
    }
    else if (t >= m_TrailClose)
    {
        // Into the recovery frames - the arm drifting back to neutral. A
        // ribbon following that reads as a second, aimless swing.
        m_SwordTrail->End();
        m_TrailRunning = false;
        return;
    }
    else if (facing != m_TrailFacing)
    {
        // Turned round mid-swing. A swing may still be redirected up to
        // m_AttackTurnWindow, which overlaps the start of the ribbon, and
        // joining the two sides would draw one long polygon straight through
        // the character. Start the ribbon again from where the blade is now.
        m_SwordTrail->Begin();
        m_TrailFacing = facing;
    }

    // Nothing to capture while the frame is held - the blade has not moved,
    // and pushing the same position in repeatedly would walk the real history
    // off the end of the buffer.
    if (!frozen)
        m_SwordTrail->Sample();
}

// Stop capturing, keep what is drawn. Shared by the two places a swing can
// begin, so neither can forget it.
void Player::EndSwingTrail()
{
    if (m_SwordTrail != nullptr)
        m_SwordTrail->End();

    m_TrailRunning = false;
}

// One burst per target the weapon reports having just damaged.
//
// The weapon decides what was hit and where; this only reads the answer. That
// separation is the point: a swing that misses records nothing, so there is
// no path from here to an impact that did not happen, and a swing that lands
// on three enemies at once gets three bursts without this knowing anything
// about reach, angles or hit radii.
void Player::SpawnImpacts()
{
    if (m_Weapon == nullptr)
        return;

    int count = m_Weapon->GetFrameHitCount();

    // The finisher and the counter land heavier - bigger burst, more sparks,
    // a little longer. Same split the damage, the hitstop and the shake
    // already use.
    bool heavy = m_SpecialAttacking || (ComboIndex(m_AttackCombo) == 2);

    float scale = SwingImpactScale();

    for (int i = 0; i < count; i++)
    {
        Manager::AddGameObj<ImpactEffect>()->Burst(
            m_Weapon->GetFrameHitPoint(i),
            m_Weapon->GetFrameHitDirection(i),
            scale, heavy);
    }
}

void Player::StartAttack()
{
    // No damage here any more - the swing lands on its active frame, see
    // the hit window in Update(). Use() at this point hit the enemy while
    // the sword was still behind the player's back.
    m_AttackHitDone = false;
    m_AttackHitFrames = 0;
    m_AttackFrame = 0.0f;
    m_SpecialAttacking = false;

    // A new swing never inherits the last one's ribbon. A combo may be
    // cancelled into from 60% of the way through a swing, which is before the
    // trail would have closed on its own - and carrying it over would join
    // the end of one swing to the windup of the next with a single polygon
    // drawn straight across the character. What is already on screen keeps
    // fading; only the capture stops.
    EndSwingTrail();

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

    // Rates first, then put the clock on the slice's first key. SetAnimation
    // only zeroes the frame when the clip NAME changes, so replaying the same
    // attack twice in a row used to carry the previous swing's frame in with
    // it - setting it here covers that too.
    SetupSwingClock(attackAnim);
    m_AttackFrame = m_AttackClipFirst;
    m_NextAnimationFrame = (int)m_AttackFrame;

    // A real crossfade, not the old near-snap.
    //
    // 0.7 was right while every swing started at key 0, which for these clips
    // is the character standing in neutral - blending into a pose it was
    // already in has nothing to hide, so snapping cost nothing. The slice now
    // starts at the COCKED pose (key 34 of Attack1: arm up and back), and
    // snapping into that in three ticks is a visible pop.
    //
    // 0.3 gives seven ticks at +0.1 a tick. The windup is thirteen, so the
    // blend finishes inside it and reads as the arm whipping up into the
    // swing rather than teleporting there.
    m_Blend = 0.3f; // skip the idle/run→attack crossfade so the pose is
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
    m_AttackHitFrames = 0;
    m_AttackFrame = 0.0f;
    m_SpecialAttacking = true;

    // Same as StartAttack - and this one can interrupt a normal swing at any
    // point at all, so it matters more here.
    EndSwingTrail();

    m_ParryTimer = m_ParryTime; // the deflect is live from the first frame
    m_AttackQueued = false;     // a press buffered before this must not chain out of it

    Vector3 forward = GetFoward();
    m_Velocity.x += forward.x * m_AttackLunge;

    m_Attacking = true;

    SoundEffect::Play(SE::SpecialAttack);

    SetAnimation("AttackRight");
    m_AttackAnimLength = m_AnimationModel->GetAnimationFrameCount("AttackRight");

    SetupSwingClock("AttackRight");
    m_AttackFrame = m_AttackClipFirst;
    m_NextAnimationFrame = (int)m_AttackFrame;

    m_Blend = 0.3f;
}