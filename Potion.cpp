#include "main.h"
#include "renderer.h"
#include "Potion.h"

#include "manager.h"
#include "modelRenderer.h"
#include "ToonShader.h"
#include "Collision.h"
#include "MeshField.h"
#include "Player.h"
#include "Stats.h"
#include "DamageNumber.h"
#include "SoundEffect.h"

// What a potion is worth, as a FRACTION of the bar it fills rather than a
// flat number. Max HP and Max MP are both reward cards - a Legendary Max HP
// roll alone is +72 - so a flat "heals 30" would start as a third of the bar
// and end the run as a rounding error.
static const float HEALTH_POTION_RATIO = 0.30f;
static const float MANA_POTION_RATIO = 0.40f;

// Both models are about 0.22 x 0.28 x 0.19 in their own space, so this puts
// a potion at roughly half a unit tall - readable on the floor next to a
// 1.8 unit player without being something you would mistake for a crate.
static const float POTION_SCALE = 1.8f;

// The models are built around their middle: y runs -0.112 to 0.167. Lifting
// the drawn mesh by that much stands it on m_Position, which is where the
// physics and the pickup test both assume it is.
static const float POTION_STAND_OFFSET = 0.112f * POTION_SCALE;

// The falling potion's own body. Small and square - it only has to land on
// things, never to fight for space.
static const Vector3 POTION_HALF_SIZE(0.25f, 0.25f, 0.25f);

static const float GRAVITY = 98.0f; // the same pull the player gets

// How close the player has to be. Generous horizontally, because walking
// past a potion and not getting it feels broken; tight vertically, so a
// potion on the floor is not hoovered up from the platform above it.
static const float PICKUP_RADIUS = 1.0f;
static const float PICKUP_HEIGHT = 1.6f;

static const float PICKUP_DELAY = 0.25f;

// Idle motion once it has settled.
static const float BOB_HEIGHT = 0.12f;
static const float BOB_SPEED = 3.0f;
static const float SPIN_SPEED = 1.8f; // radians per second

static const XMFLOAT4 HEALTH_COLOUR = XMFLOAT4(0.35f, 1.0f, 0.40f, 1.0f);
static const XMFLOAT4 MANA_COLOUR = XMFLOAT4(0.40f, 0.65f, 1.0f, 1.0f);

void Potion::Init()
{
    m_Layer = 1; // with the props, not the effects
    m_Scale = { POTION_SCALE, POTION_SCALE, POTION_SCALE };
}

void Potion::Spawn(PotionType Type, const Vector3& Position, const Vector3& Velocity)
{
    m_Type = Type;
    m_Position = Position;
    m_Velocity = Velocity;
    m_PickupDelay = PICKUP_DELAY;

    // These are Wavefront OBJ, so they go through ModelRenderer rather than
    // AnimationModel - the opposite way round from the crate that drops
    // them. Each .obj names its own .mtl, which names Color.png beside it,
    // and ModelRenderer resolves both relative to the model's folder.
    m_Model = AddGameComponent<ModelRenderer>(this);
    m_Model->Load(m_Type == PotionType::Health
        ? "asset\\model\\Potion\\Red_Potion_3.obj"
        : "asset\\model\\Potion\\Blue_Potion_3.obj");
}

void Potion::Uninit()
{
    GameObject::Uninit();
}

void Potion::Update()
{
    const float dt = 1.0f / 60.0f;

    if (m_PickupDelay > 0.0f)
        m_PickupDelay -= dt;

    if (!m_Grounded)
    {
        m_Velocity.y += -GRAVITY * dt;

        // One axis at a time against the same solids the player walks on, so
        // a potion thrown against a platform slides down its face instead of
        // sinking into it, and one thrown onto a platform rests on top.
        std::vector<AABB> solids = Collision::GatherSolids();

        if (Collision::MoveX(m_Position, POTION_HALF_SIZE, m_Velocity.x * dt, solids))
            m_Velocity.x = 0.0f;

        bool landed = false;
        if (Collision::MoveY(m_Position, POTION_HALF_SIZE, m_Velocity.y * dt, solids, landed))
            m_Velocity.y = 0.0f;

        // The terrain underneath, exactly as Player::Update does it - the
        // ground is a mesh, not the y = 0 plane.
        MeshField* meshField = Manager::GetGameObj<MeshField>();

        if (meshField != nullptr)
        {
            float height = meshField->GetHeight(m_Position);

            if (m_Position.y < height)
            {
                m_Position.y = height;
                m_Velocity.y = 0.0f;
                landed = true;
            }
        }

        if (landed)
        {
            m_Grounded = true;
            m_Velocity = { 0.0f, 0.0f, 0.0f };
        }
    }
    else
    {
        m_IdleTime += dt;
    }

    TryCollect();

    GameObject::Update();
}

void Potion::TryCollect()
{
    if (m_Collected || m_PickupDelay > 0.0f)
        return;

    Player* player = Manager::GetGameObj<Player>();

    if (player == nullptr)
        return;

    Stats* stats = player->GetGameComponent<Stats>();

    if (stats == nullptr)
        return;

    // 2.5D, like every other reach test here: horizontal distance against
    // one limit, height against another. Measuring a single 3D distance
    // would make a potion harder to pick up while jumping, for no reason a
    // player could see.
    Vector3 toPlayer = player->GetPosition() - m_Position;

    float dy = toPlayer.y;
    toPlayer.y = 0.0f;

    if (toPlayer.lenght() > PICKUP_RADIUS)
        return;

    if (dy > PICKUP_HEIGHT || dy < -PICKUP_HEIGHT)
        return;

    int amount;

    if (m_Type == PotionType::Health)
    {
        amount = (int)(stats->GetMaxHP() * HEALTH_POTION_RATIO + 0.5f);
        if (amount < 1)
            amount = 1;

        stats->Heal(amount);
    }
    else
    {
        amount = (int)(stats->GetMaxMP() * MANA_POTION_RATIO + 0.5f);
        if (amount < 1)
            amount = 1;

        stats->RestoreMP(amount);
    }

    m_Collected = true;

    SoundEffect::Play(SE::PotionPickup);

    // Above the player's head rather than over the potion, so the number
    // reads as something that happened to the player. ShowSign, so a heal
    // cannot be mistaken for the damage numbers the enemies throw.
    Vector3 popupPos = player->GetPosition();
    popupPos.y += 2.2f;

    DamageNumber* popup = Manager::AddGameObj<DamageNumber>();
    popup->Init(popupPos, amount, true,
        m_Type == PotionType::Health ? HEALTH_COLOUR : MANA_COLOUR);

    SetDestory();
}

void Potion::Draw()
{
    ToonShader::Bind(ToonShader::SceneryLook);

    // Stand the mesh on m_Position, then bob and spin it. All three are
    // applied here rather than written into m_Position, so the pickup test
    // above always measures against where the potion really is - a bobbing
    // position would make the reach drift with the animation.
    float bob = m_Grounded ? sinf(m_IdleTime * BOB_SPEED) * BOB_HEIGHT : 0.0f;
    float spin = m_IdleTime * SPIN_SPEED;

    XMMATRIX world =
        XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z) *
        XMMatrixTranslation(0.0f, POTION_STAND_OFFSET, 0.0f) *
        XMMatrixRotationY(spin) *
        XMMatrixTranslation(m_Position.x, m_Position.y + bob, m_Position.z);

    Renderer::SetWorldMatrix(world);

    for (Component* component : m_Components)
        component->Draw();
}
