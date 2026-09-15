#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "SlashEffect.h"
#include "Camera.h"

// ---------------------------------------------------------------------------
// The sprite.
//
// A crescent, drawn bulging to the RIGHT with its tips tapering to points and
// a hot flare at the leading tip. The curve has to live in the texture: a
// flat quad can be moved, turned and stretched, but it can never be bent, so
// no amount of animation turns a round blob into an arc. (That is what the
// earlier trail.png version got wrong - it could only ever be a straight
// streak.) Swap in any crescent png here and the rest still works.
// ---------------------------------------------------------------------------
#define SLASH_TEXTURE  L"asset\\texture\\slash_crescent.png"

// Fraction of the texture the quad samples, from the centre. The crescent is
// drawn with its own margin, so it takes the whole image.
#define SLASH_UV_KEEP  1.0f

// ---------------------------------------------------------------------------
// The shape of a swing, over its lifetime (t goes 0 -> 1).
//
// Length grows while thickness collapses, so the streak starts as a stubby
// bright mass and whips out into a thin line - that stretch is what reads as
// speed. The alpha snaps on over the first few percent and then falls away on
// a curve, so it lands like a glint rather than fading politely.
// ---------------------------------------------------------------------------
// The arc opens out as it travels and then burns away. It is scaled almost
// evenly on both axes: the crescent's shape is baked into the texture, so
// pulling the axes apart distorts the curve instead of shaping the swing.
static const float SWING_LENGTH_START = 0.78f;  // x scale multiplier at t=0
static const float SWING_LENGTH_END   = 1.14f;  // ... and at t=1
static const float SWING_THICK_START  = 0.74f;  // y scale multiplier at t=0
static const float SWING_THICK_END    = 1.12f;  // ... and at t=1
static const float SWING_ATTACK       = 0.10f;  // fraction of life spent fading in
static const float SWING_DECAY        = 1.7f;   // higher = the tail dies faster
static const float SWING_GAIN         = 1.5f;   // >1 widens the blown-out core

// ---------------------------------------------------------------------------
// Shared GPU resources.
//
// Explosion and DamageNumber load their texture and compile their shaders in
// every Init(), which costs a few milliseconds per spawn. That is survivable
// for something that appears rarely; a slash fires three times per combo and
// pulls in thirteen 500x500 textures, so these are loaded once and kept for
// the life of the program rather than per object or per scene. Manager::Uninit
// releases them.
// ---------------------------------------------------------------------------
static ID3D11Buffer*             s_VertexBuffer = nullptr;
static ID3D11InputLayout*        s_VertexLayout = nullptr;
static ID3D11VertexShader*       s_VertexShader = nullptr;
static ID3D11PixelShader*        s_PixelShader = nullptr;
static ID3D11ShaderResourceView* s_Texture = nullptr;
static bool                      s_Loaded = false;

void SlashEffect::LoadShared()
{
    if (s_Loaded)
        return;

    s_Loaded = true;

    // A unit quad centred on the origin, so the spawn position is the middle
    // of the slash rather than a corner. The matrix does the scaling, so the
    // vertices never have to be rewritten - unlike Explosion, which re-maps
    // its buffer every frame to animate UVs. Here the frame changes by
    // swapping the texture, so the UVs stay put too.
    const float uv0 = 0.5f - SLASH_UV_KEEP * 0.5f;
    const float uv1 = 0.5f + SLASH_UV_KEEP * 0.5f;

    VERTEX_3D vertex[4];

    vertex[0].Position = XMFLOAT3(-1.0f, 1.0f, 0.0f);
    vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[0].TexCoord = XMFLOAT2(uv0, uv0);

    vertex[1].Position = XMFLOAT3(1.0f, 1.0f, 0.0f);
    vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[1].TexCoord = XMFLOAT2(uv1, uv0);

    vertex[2].Position = XMFLOAT3(-1.0f, -1.0f, 0.0f);
    vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[2].TexCoord = XMFLOAT2(uv0, uv1);

    vertex[3].Position = XMFLOAT3(1.0f, -1.0f, 0.0f);
    vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[3].TexCoord = XMFLOAT2(uv1, uv1);

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DEFAULT; // never rewritten, so it need not be dynamic
    bd.ByteWidth = sizeof(VERTEX_3D) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertex;

    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &s_VertexBuffer);

    Renderer::CreateVertexShader(&s_VertexShader, &s_VertexLayout,
        "shader\\unlitTextureVS.cso");

    Renderer::CreatePixelShader(&s_PixelShader,
        "shader\\unlitTexturePS.cso");

    TexMetadata metadata;
    ScratchImage image;

    if (FAILED(LoadFromWICFile(SLASH_TEXTURE, WIC_FLAGS_NONE, &metadata, image)))
    {
        // Missing texture is not fatal - Draw checks for it and skips.
        OutputDebugStringA("SlashEffect: could not load the slash texture\n");
        return;
    }

    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &s_Texture);
}

void SlashEffect::UninitShared()
{
    if (s_Texture)      { s_Texture->Release();      s_Texture = nullptr; }

    if (s_VertexBuffer) { s_VertexBuffer->Release(); s_VertexBuffer = nullptr; }
    if (s_VertexLayout) { s_VertexLayout->Release(); s_VertexLayout = nullptr; }
    if (s_VertexShader) { s_VertexShader->Release(); s_VertexShader = nullptr; }
    if (s_PixelShader)  { s_PixelShader->Release();  s_PixelShader = nullptr; }

    s_Loaded = false;
}

void SlashEffect::Init()
{
    m_Layer = 3; // the VFX layer - same as Particle and Explosion

    LoadShared(); // no-op once the frames are in
}

void SlashEffect::Uninit()
{
    // Nothing per object: the frames, buffer and shaders are shared and
    // outlive every individual slash. Manager::Uninit frees them.
    GameObject::Uninit();
}

void SlashEffect::Play(const Vector3& Position, const Vector3& Rotation, const Vector3& Scale,
    float Lifetime, float Alpha, float Sweep)
{
    // Straight onto the GameObject's own transform - GetMatrx() turns these
    // into the world matrix in Draw, so there is no separate maths here and
    // all three rotation axes behave the way they do everywhere else.
    m_Position = Position;
    m_Rotation = Rotation;
    m_Scale = Scale;
    m_StartAlpha = Alpha;
    m_Sweep = Sweep;

    // The animation works off these, so the per-frame stretch and the sweep
    // are always measured from where the swing started rather than from
    // wherever the last frame left them.
    m_BaseScale = Scale;
    m_BaseRoll = Rotation.z;

    if (Lifetime > 0.0f)
        m_Lifetime = Lifetime;

    m_Timer = 0.0f;
}

void SlashEffect::FollowWeapon(GameObject* Weapon, float Reach, float Depth)
{
    m_Follow = Weapon;
    m_FollowReach = Reach;
    m_FollowDepth = Depth;

    // The blade supplies the sweep now, so the baked-in one would double it.
    m_Sweep = 0.0f;

    m_HasFollowRoll = false;
}

void SlashEffect::Update()
{
    const float dt = 1.0f / 60.0f;

    m_Timer += dt;

    if (m_Timer >= m_Lifetime)
    {
        SetDestory();
        return;
    }

    if (m_Follow != nullptr)
    {
        // Row 3 of the world matrix is the grip in world space; row 2 is the
        // blade's own axis, with the socket's rotation and the player's turn
        // already folded in. sword.fbx measures 4.3x longer down its local Z
        // than anything else, which is why that row is the blade.
        XMMATRIX world = m_Follow->GetMatrx();

        Vector3 grip;
        XMStoreFloat3((XMFLOAT3*)&grip, world.r[3]);

        XMFLOAT3 blade;
        XMStoreFloat3(&blade, XMVector3Normalize(world.r[2]));

        m_Position = grip + Vector3(blade.x, blade.y, blade.z) * m_FollowReach;
        m_Position.z += m_FollowDepth;

        // Measured in world space, so nothing needs mirroring when the player
        // turns around - the sword has already turned with him.
        float flat = sqrtf(blade.x * blade.x + blade.y * blade.y);

        if (flat > m_FollowRollMin)
        {
            m_FollowRoll = atan2f(blade.y, blade.x);
            m_HasFollowRoll = true;
        }

        if (m_HasFollowRoll)
            m_BaseRoll = m_FollowRoll;
    }

    GameObject::Update();
}

void SlashEffect::Draw()
{
    if (s_VertexBuffer == nullptr)
        return;

    Camera* camera = Manager::GetGameObj<Camera>();
    if (camera == nullptr)
        return; // no camera yet - nothing to billboard against

    float t = m_Timer / m_Lifetime;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    if (s_Texture == nullptr)
        return; // texture never loaded - nothing to draw

    // The swing, made out of the transform rather than out of frames:
    // the streak lengthens, thins, and rotates through its sweep, all in the
    // time the swing takes. Done here rather than in Update so the shape is
    // always derived from t and never drifts.
    m_Scale.x = m_BaseScale.x * (SWING_LENGTH_START + (SWING_LENGTH_END - SWING_LENGTH_START) * t);
    m_Scale.y = m_BaseScale.y * (SWING_THICK_START + (SWING_THICK_END - SWING_THICK_START) * t);
    m_Rotation.z = m_BaseRoll + m_Sweep * (t - 0.5f);

    Renderer::GetDeviceContext()->IASetInputLayout(s_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(s_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(s_PixelShader, NULL, 0);

    Renderer::SetDepthEnable(m_DepthTest);
    Renderer::SetAddBlendEnable(m_Additive);

    // Without this, any orientation that turns the quad past edge-on shows
    // its back face and gets culled - half of every yaw and pitch sweep
    // would blink out. Restored at the end of Draw.
    Renderer::SetCullEnable(false);

    // A genuine world-space plane: scale * RollPitchYaw * translate, built by
    // the base class from m_Position / m_Rotation / m_Scale. This used to
    // billboard, which forced the quad flat to the camera and threw away
    // pitch and yaw entirely - the arc could only ever spin in the screen
    // plane, which is what made it look like a sticker rather than a swing
    // travelling through the scene.
    Renderer::SetWorldMatrix(GetMatrx());

    // unlitTextureVS multiplies Material.Diffuse into the vertex colour and
    // the pixel shader multiplies that into the sampled texel, so this alpha
    // reaches the blend stage with no shader change.
    // Snap on, then fall away on a curve. A linear fade reads as the effect
    // politely disappearing; this reads as a glint. Gain above 1 does not
    // make it brighter than white - it widens the part of the streak that is
    // fully blown out, which is what gives it a hot core and a soft tip.
    float alpha;
    if (t < SWING_ATTACK)
        alpha = t / SWING_ATTACK;
    else
        alpha = powf(1.0f - (t - SWING_ATTACK) / (1.0f - SWING_ATTACK), SWING_DECAY);

    alpha *= m_StartAlpha * SWING_GAIN;

    if (m_FadeOut)
        alpha *= (1.0f - t);

    MATERIAL material{};
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, alpha);
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.TextureEnable = true;

    Renderer::SetMaterial(material);

    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &s_Texture);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;

    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &s_VertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    Renderer::GetDeviceContext()->Draw(4, 0);

    // Put the pipeline back the way everything else expects it - Particle
    // does the same after its additive pass.
    Renderer::SetCullEnable(true);
    Renderer::SetDepthEnable(true);
    Renderer::SetAddBlendEnable(false);
}
