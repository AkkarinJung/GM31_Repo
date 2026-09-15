#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "SwordTrail.h"

// A soft white blob, already in the project (SlashEffect's header calls it
// exactly that). Sampled down its own centre column - see the UVs in Draw -
// it gives the ribbon a blown-out core with both edges fading to nothing,
// which is the whole reason it is a texture at all rather than flat colour.
#define TRAIL_TEXTURE  L"asset\\texture\\trail.png"

// Where across the texture the two edges of the ribbon sample. Kept off 0 and
// 1 so the edge texels - which are fully transparent - are not what gets
// stretched along the whole length.
static const float TRAIL_U = 0.5f;
static const float TRAIL_V_BASE = 0.06f;
static const float TRAIL_V_TIP = 0.94f;

// Below this the strip is a line, not a surface. One sample is a swing that
// has only just started; drawing it would be a single flat quad sitting on
// the blade, which reads as a glitch rather than as a trail.
static const int TRAIL_MIN_SAMPLES = 2;

void SwordTrail::Init()
{
    m_Layer = 3; // the VFX layer - same as Particle, Explosion and SlashEffect

    // Two vertices per sample: the strip is base/tip pairs walked from the
    // blade backwards. DYNAMIC because every vertex moves every frame - the
    // ribbon is rebuilt in world space in Draw, not transformed by a matrix.
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(VERTEX_3D) * TRAIL_MAX * 2;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    Renderer::GetDevice()->CreateBuffer(&bd, nullptr, &m_VertexBuffer);

    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\unlitTextureVS.cso");

    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\unlitTexturePS.cso");

    TexMetadata metadata;
    ScratchImage image;

    if (SUCCEEDED(LoadFromWICFile(TRAIL_TEXTURE, WIC_FLAGS_NONE, &metadata, image)))
    {
        CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
            image.GetImageCount(), metadata, &m_Texture);
    }
    else
    {
        // Not fatal. Draw falls back to flat vertex colour, which is a harder
        // edged ribbon but still a ribbon - better than a swing with nothing
        // behind it because one png went missing.
        OutputDebugStringA("SwordTrail: could not load the trail texture\n");
    }
}

void SwordTrail::Uninit()
{
    if (m_Texture) { m_Texture->Release(); m_Texture = nullptr; }
    if (m_VertexBuffer) { m_VertexBuffer->Release(); m_VertexBuffer = nullptr; }
    if (m_VertexLayout) { m_VertexLayout->Release(); m_VertexLayout = nullptr; }
    if (m_VertexShader) { m_VertexShader->Release(); m_VertexShader = nullptr; }
    if (m_PixelShader) { m_PixelShader->Release();  m_PixelShader = nullptr; }

    GameObject::Uninit();
}

void SwordTrail::SetBlade(GameObject* Blade, float BaseReach, float TipReach)
{
    m_Blade = Blade;
    m_BaseReach = BaseReach;
    m_TipReach = TipReach;
}

void SwordTrail::Begin()
{
    Clear();
    m_Emitting = true;
}

void SwordTrail::End()
{
    m_Emitting = false;
}

void SwordTrail::Clear()
{
    m_SampleCount = 0;
    m_Emitting = false;
}

void SwordTrail::Sample()
{
    if (!m_Emitting || m_Blade == nullptr)
        return;

    // Row 3 of the world matrix is the grip in world space and row 2 is the
    // blade's own long axis, with the hand socket and the player's facing
    // already folded in. The same two rows SlashEffect::FollowWeapon reads,
    // and for the same reason: sword.fbx is far longer down its local Z than
    // it is on either other axis, so that row IS the blade.
    XMMATRIX world = m_Blade->GetMatrx();

    Vector3 grip;
    XMStoreFloat3((XMFLOAT3*)&grip, world.r[3]);

    XMFLOAT3 axis;
    XMStoreFloat3(&axis, XMVector3Normalize(world.r[2]));

    Vector3 blade(axis.x, axis.y, axis.z);

    // Oldest sample falls off the end. A memmove of two dozen small structs
    // once a frame is cheaper than the bookkeeping a ring buffer would need
    // here, and the array stays in draw order - newest first - which is what
    // the strip below walks.
    if (m_SampleCount < TRAIL_MAX)
        m_SampleCount++;

    for (int i = m_SampleCount - 1; i > 0; i--)
        m_Sample[i] = m_Sample[i - 1];

    m_Sample[0].Base = grip + blade * m_BaseReach;
    m_Sample[0].Tip = grip + blade * m_TipReach;
    m_Sample[0].Age = 0.0f;

    // Keeps the depth sort honest - Manager orders every object by its
    // distance along the camera's forward axis, and an object left at the
    // world origin sorts as if it were there.
    m_Position = m_Sample[0].Tip;
}

void SwordTrail::Update()
{
    const float dt = 1.0f / 60.0f;

    // Held by the impact freeze: keep the ribbon exactly where it is. The
    // blade is not moving either, so there is nothing to lose by stopping.
    if (m_Frozen)
    {
        GameObject::Update();
        return;
    }

    // Age everything, then drop the tail once it has outlived the fade. The
    // array is newest-first, so everything past the first dead sample is
    // older still and goes with it.
    for (int i = 0; i < m_SampleCount; i++)
    {
        m_Sample[i].Age += dt;

        if (m_Sample[i].Age >= m_SampleLife)
        {
            m_SampleCount = i;
            break;
        }
    }

    GameObject::Update();
}

void SwordTrail::Draw()
{
    if (m_VertexBuffer == nullptr || m_SampleCount < TRAIL_MIN_SAMPLES)
        return;

    D3D11_MAPPED_SUBRESOURCE msr{};
    if (FAILED(Renderer::GetDeviceContext()->Map(m_VertexBuffer, 0,
        D3D11_MAP_WRITE_DISCARD, 0, &msr)))
        return;

    VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;

    const float lastIndex = (float)(m_SampleCount - 1);

    for (int i = 0; i < m_SampleCount; i++)
    {
        const TrailSample& sample = m_Sample[i];

        // Two independent curves, and they have to stay independent.
        //
        //   along  is where this sample sits on the ribbon, 0 at the blade
        //          and 1 at the tail. It shapes the TAPER and the colour.
        //   fade   is how long ago it was captured. It shapes the ALPHA.
        //
        // They look like the same number while the blade is still emitting,
        // and they stop being the same the instant it stops: the ribbon then
        // has to keep its shape while every part of it dies, and a taper
        // driven off age would make the whole thing collapse to a point.
        float along = (lastIndex > 0.0f) ? (i / lastIndex) : 0.0f;
        float fade = 1.0f - (sample.Age / m_SampleLife);

        if (fade < 0.0f) fade = 0.0f;
        if (fade > 1.0f) fade = 1.0f;

        float alpha = powf(fade, m_FadePower) * m_Gain;

        // Narrow towards the tail: the far edge is pulled back in along the
        // blade so the strip closes to a point instead of ending in a
        // rectangle.
        float width = 1.0f - (1.0f - m_TailWidth) * along;

        Vector3 base = sample.Base;
        Vector3 tip = base + (sample.Tip - base) * width;

        XMFLOAT4 colour;
        colour.x = m_HeadColour.x + (m_TailColour.x - m_HeadColour.x) * along;
        colour.y = m_HeadColour.y + (m_TailColour.y - m_HeadColour.y) * along;
        colour.z = m_HeadColour.z + (m_TailColour.z - m_HeadColour.z) * along;
        colour.w = alpha;

        VERTEX_3D& baseVertex = vertex[i * 2];
        VERTEX_3D& tipVertex = vertex[i * 2 + 1];

        baseVertex.Position = XMFLOAT3(base.x, base.y, base.z);
        baseVertex.Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
        baseVertex.Diffuse = colour;

        // U is the SAME on both edges on purpose. It pins the strip to one
        // vertical column of the blob, so the shading across the ribbon is
        // the alpha profile of that column - transparent, blown out,
        // transparent - and it stays identical over the whole length however
        // far the blade travelled. Running U along the ribbon instead would
        // slide the bright part of the texture up and down the streak as the
        // swing stretched it.
        baseVertex.TexCoord = XMFLOAT2(TRAIL_U, TRAIL_V_BASE);

        tipVertex.Position = XMFLOAT3(tip.x, tip.y, tip.z);
        tipVertex.Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
        tipVertex.Diffuse = colour;
        tipVertex.TexCoord = XMFLOAT2(TRAIL_U, TRAIL_V_TIP);
    }

    Renderer::GetDeviceContext()->Unmap(m_VertexBuffer, 0);

    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    // Same three pieces of state SlashEffect and Particle set, for the same
    // reasons: additive so the streak reads as light, no depth test so it is
    // never swallowed by the body it is sweeping past, and no culling so the
    // strip cannot vanish when the blade turns its back to the camera part
    // way through a swing. All three are put back at the end.
    Renderer::SetDepthEnable(false);
    Renderer::SetAddBlendEnable(true);
    Renderer::SetCullEnable(false);

    // The vertices are already in world space - they are captured blade
    // positions, not a shape to be placed - so the world matrix is identity.
    Renderer::SetWorldMatrix(XMMatrixIdentity());

    MATERIAL material{};
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f); // the per-vertex
                                                         // colour carries the
                                                         // fade; this must not
                                                         // scale it a second time
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.TextureEnable = (m_Texture != nullptr);

    Renderer::SetMaterial(material);

    if (m_Texture != nullptr)
        Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;

    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    Renderer::GetDeviceContext()->Draw(m_SampleCount * 2, 0);

    Renderer::SetCullEnable(true);
    Renderer::SetDepthEnable(true);
    Renderer::SetAddBlendEnable(false);
}
