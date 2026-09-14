#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Hedge.h"
#include "animationModel.h"

// What one segment should come out as in world units, rather than a multiplier
// on whatever the file happens to be in. hedge_straight_long.fbx carries an
// Lcl Scaling of 100 on its model node, which AnimationModel bakes into the
// vertices, so it arrives 400 units long - a multiplier of 1 filled the screen.
// Sizing to a target instead means the FBX's own units stop mattering.
static const float HEDGE_LENGTH = 4.0f;

// Twice the player's 1.8, so it reads as a wall rather than something to
// vault. At the model's own proportions it would come out shorter than the
// player and look like scenery.
static const float HEDGE_HEIGHT = 3.6f;

// How high the invisible wall reaches, which is deliberately nothing to do
// with how tall the hedge looks. The +15% Jump Power reward multiplies and
// stacks every stage - one pick already clears 3.6, six clear 17 - so there
// is no hedge height that stays un-jumpable. Tying the collider to the model
// is right for a crate and wrong for the edge of the map.
static const float HEDGE_SOLID_HEIGHT = 200.0f;

void Hedge::Init()
{
    m_Layer = 1;

    // ModelRenderer only parses Wavefront OBJ, so FBX props go through
    // AnimationModel - the same as Box and Sword.
    AnimationModel* animationModel = AddGameComponent<AnimationModel>(this);
    animationModel->Load("asset\\model\\Enviroment\\hedge_straight_long.fbx");

    XMFLOAT3 boundsMin = animationModel->GetBoundsMin();
    XMFLOAT3 boundsMax = animationModel->GetBoundsMax();

    m_ModelHalfSize = Vector3((boundsMax.x - boundsMin.x) * 0.5f,
                              (boundsMax.y - boundsMin.y) * 0.5f,
                              (boundsMax.z - boundsMin.z) * 0.5f);

    // Length drives x and z together so the hedge keeps its thickness against
    // its length; height is set on its own, which is what makes it tall.
    float lengthScale = (m_ModelHalfSize.x > 0.0001f)
        ? HEDGE_LENGTH / (m_ModelHalfSize.x * 2.0f) : 1.0f;
    float heightScale = (m_ModelHalfSize.y > 0.0001f)
        ? HEDGE_HEIGHT / (m_ModelHalfSize.y * 2.0f) : 1.0f;

    m_Scale = { lengthScale, heightScale, lengthScale };

    // AnimationModel::Draw sets its own material and texture but not the
    // shaders, so this has to bind them itself - the same as Box.
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\unlitTextureVS.cso");

    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\unlitTexturePS.cso");
}

void Hedge::Uninit()
{
    if (m_VertexLayout) m_VertexLayout->Release();
    if (m_VertexShader) m_VertexShader->Release();
    if (m_PixelShader) m_PixelShader->Release();

    GameObject::Uninit();
}

void Hedge::Update()
{
    GameObject::Update();
}

Vector3 Hedge::GetSolidHalfSize() const
{
    float halfX = m_ModelHalfSize.x * m_Scale.x;
    float halfZ = m_ModelHalfSize.z * m_Scale.z;

    // Height comes from the wall, not the model - see HEDGE_SOLID_HEIGHT.
    float halfY = HEDGE_SOLID_HEIGHT * 0.5f;

    // Scale happens before rotation, so a quarter turn about Y swaps which way
    // the long side of the hedge runs. Without this the box is the right size
    // pointing the wrong way, and the player walks through the end wall.
    if (fabsf(sinf(m_Rotation.y)) > 0.5f)
    {
        float swap = halfX;
        halfX = halfZ;
        halfZ = swap;
    }

    return Vector3(halfX, halfY, halfZ);
}

void Hedge::Draw()
{
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    GameObject::Draw();
}
