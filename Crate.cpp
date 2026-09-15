#include "main.h"
#include "renderer.h"
#include "Crate.h"

#include "manager.h"
#include "animationModel.h"
#include "ToonShader.h"
#include "Explosion.h"
#include "Potion.h"
#include "SoundEffect.h"

// How the drop is decided, out of 100. Health and mana are equally likely
// and most crates still give something, which is what keeps a crate worth
// the swing - see Break().
static const int DROP_HEALTH_CHANCE = 33;
static const int DROP_MANA_CHANCE = 33;

// How hard a potion is thrown clear of the crate that held it. Enough to
// read as "it popped out", not enough to lose it behind the scenery: at
// 6 up it clears about 0.18 units before gravity takes it back.
static const float POTION_POP_UP = 6.0f;
static const float POTION_POP_SIDE = 1.5f;

void Crate::Init()
{
    m_Layer = 1; // with the terrain and the platforms, not the effects

    // Half a unit each way, so the crate is one unit across and one tall:
    // small enough to read as a prop rather than a platform, and low enough
    // to hop straight over when one sits in the way.
    m_Scale = { 0.5f, 0.5f, 0.5f };

    // ModelRenderer only parses Wavefront OBJ, so an FBX prop rides on
    // AnimationModel - the same route Box, Sword and Hedge take.
    m_Model = AddGameComponent<AnimationModel>(this);
    m_Model->Load("asset\\model\\Potion\\crate.fbx");

    // The collision treats a crate as spanning -1..1 in x and z and 0..2 in
    // y before this object's scale, so measure what actually loaded and map
    // the model onto that. Measured rather than typed in, so re-exporting
    // the crate at another size cannot silently break it.
    XMFLOAT3 boundsMin = m_Model->GetBoundsMin();
    XMFLOAT3 boundsMax = m_Model->GetBoundsMax();

    Vector3 size(boundsMax.x - boundsMin.x,
                 boundsMax.y - boundsMin.y,
                 boundsMax.z - boundsMin.z);

    m_FitScale.x = (size.x > 0.0001f) ? 2.0f / size.x : 1.0f;
    m_FitScale.y = (size.y > 0.0001f) ? 2.0f / size.y : 1.0f;
    m_FitScale.z = (size.z > 0.0001f) ? 2.0f / size.z : 1.0f;

    // Centre it on x and z, and stand it on y.
    m_FitOffset.x = -(boundsMin.x + boundsMax.x) * 0.5f * m_FitScale.x;
    m_FitOffset.y = -boundsMin.y * m_FitScale.y;
    m_FitOffset.z = -(boundsMin.z + boundsMax.z) * 0.5f * m_FitScale.z;
}

void Crate::Uninit()
{
    GameObject::Uninit();
}

void Crate::Update()
{
    // Decay the wobble, written exactly the way Enemy::Update does it: the
    // offset alternates sign every frame rather than riding a cosine, and it
    // ends when the amplitude runs out rather than on a timer. A frame flip
    // reads as a vibration at 60fps and averages to zero; a cosine sampled
    // every 1.667 radians does neither.
    m_ShakeOffset = m_Shake * ((m_ShakeFlip & 1) ? -1.0f : 1.0f);
    m_ShakeFlip++;
    m_Shake *= m_ShakeDecay;

    if (m_Shake.lenght() < 0.001f)
    {
        m_Shake = Vector3(0.0f, 0.0f, 0.0f);
        m_ShakeOffset = Vector3(0.0f, 0.0f, 0.0f);
    }

    GameObject::Update();
}

void Crate::AddDamage(int Damage, bool Critical)
{
    (void)Critical; // a crate has no damage number - it either holds or bursts

    if (m_Broken)
        return;

    m_HP -= Damage;

    if (m_HP > 0)
        return;

    Break();
}

void Crate::Break()
{
    m_Broken = true;

    // Played from here rather than from anything attached to this object:
    // SetDestory() below tears the crate down, and a voice it owned would be
    // cut off in the same frame. The same reason Enemy plays its death sound
    // from Enemy rather than from its own Audio component.
    SoundEffect::Play(SE::CrateBreak);

    // The middle of the crate, not its feet - the burst should come from
    // where the box actually was.
    Vector3 centre = m_Position;
    centre.y += m_Scale.y;

    Explosion* explosion = Manager::AddGameObj<Explosion>();
    explosion->SetPosition(centre);

    // The drop table. One roll out of 100 so the three cases read as the
    // percentages they are: 33 health, 33 mana, and the remaining 34 empty.
    // An empty crate is the majority of any single pair, which is what stops
    // a row of crates from simply refilling both bars.
    int roll = rand() % 100;

    PotionType type;

    if (roll < DROP_HEALTH_CHANCE)
        type = PotionType::Health;
    else if (roll < DROP_HEALTH_CHANCE + DROP_MANA_CHANCE)
        type = PotionType::Mana;
    else
    {
        SetDestory(); // empty - the burst and the sound are the whole story
        return;
    }

    // Thrown clear rather than dropped on the spot, so the pickup is visibly
    // a thing that came out of the crate. Which way it leans is random, so a
    // row of crates does not spit every potion the same direction.
    float side = ((rand() % 2) ? POTION_POP_SIDE : -POTION_POP_SIDE);

    // Out of the TOP, not the middle. Manager reaps destroyed objects after
    // the update loop, so this crate is still in Collision::GatherSolids for
    // the rest of this frame - and an object created during that loop is
    // visited by it. A potion born at the crate's centre would therefore
    // take its first physics step inside a solid. Standing it on the lid
    // puts it clear of the box it came out of, and since it leaves with
    // upward velocity it is well away before the crate stops being solid.
    Vector3 spawnPos = m_Position;
    spawnPos.y += m_Scale.y * 2.0f;

    Potion* potion = Manager::AddGameObj<Potion>();
    potion->Spawn(type, spawnPos, { side, POTION_POP_UP, 0.0f });

    SetDestory();
}

void Crate::Draw()
{
    ToonShader::Bind(ToonShader::SceneryLook);

    // Same as GameObject::Draw, with the model fit from Init applied in
    // front of the object's own transform and the hit wobble in front of
    // that - so the shake moves the picture without ever moving the solid
    // the player is standing against.
    XMMATRIX fit = XMMatrixScaling(m_FitScale.x, m_FitScale.y, m_FitScale.z)
        * XMMatrixTranslation(m_FitOffset.x, m_FitOffset.y, m_FitOffset.z);

    XMMATRIX shake = XMMatrixTranslation(m_ShakeOffset.x, m_ShakeOffset.y, m_ShakeOffset.z);

    Renderer::SetWorldMatrix(fit * GetMatrx() * shake);

    for (Component* component : m_Components)
        component->Draw();
}
