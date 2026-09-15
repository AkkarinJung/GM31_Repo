#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "SlashArc.h"

// ---------------------------------------------------------------------------
// The crescent, generated once.
//
// Built on a circle so the curve is exact, then scaled so the chord runs from
// x = -1 to x = +1 with the belly on +Y. Both x and y are divided by the same
// number, so the shape stays a true circular arc - which is what lets the
// surface normal below be the plain radial direction rather than something
// that has to be differentiated.
//
//   theta(u) = (u - 0.5) * ARC_SPREAD          u walks 0 -> 1 along the arc
//   centre   = ( sin t, cos t - cos(S/2) ) / sin(S/2)
//   normal   = ( sin t, cos t )
//
// At u = 0 and u = 1 the centre sits at x = -1 and x = +1 with y = 0; at
// u = 0.5 it reaches the top of the belly. ARC_SPREAD is the only thing that
// decides how deep that belly is, before the caller's Bow scales it.
// ---------------------------------------------------------------------------

// How much of a circle the crescent covers. Wider is a rounder, fatter
// crescent; narrower flattens towards a straight streak. Two radians is a
// belly of about 0.55 against a half-chord of 1, which is the proportion a
// drawn slash usually has.
static const float ARC_SPREAD = 2.0f;

// Segments along the arc. The wipe-on advances a segment at a time, so this
// is also how smoothly the cut extends - below about sixteen the leading tip
// visibly steps.
static const int ARC_SEGMENTS = 28;

// Half the ribbon's width at its thickest, before the caller's scale. The
// taper below takes it to zero at both ends.
static const float ARC_HALF_WIDTH = 0.20f;

// How sharply the ends come to a point. The width follows sin(pi*u) raised to
// this: at 1 the taper is a plain sine, and below 1 the arc stays fat further
// out and then closes quickly - which is the sharp, drawn-with-one-stroke tip
// this is after.
static const float ARC_TIP_SHARPNESS = 0.65f;

// ---- how the cut behaves over its life (t goes 0 -> 1) --------------------

// The wipe. The arc draws itself on from the leading tip over this fraction
// of its life, then holds. Short, because it is the swing: stretched out it
// stops being a cut and becomes a wipe transition.
static const float ARC_WIPE = 0.30f;

// Stretch. The cut lengthens slightly as it travels, which is the cheapest
// thing that reads as speed.
static const float ARC_LENGTH_START = 0.88f;
static const float ARC_LENGTH_END = 1.12f;

// And thins as it does, so the belly tightens rather than inflating.
static const float ARC_BOW_START = 1.10f;
static const float ARC_BOW_END = 0.86f;

// Alpha. Snaps on over the wipe and then falls away on a curve - a linear
// fade reads as the effect politely disappearing, this reads as a glint.
static const float ARC_DECAY = 1.5f;

// Additive, so above 1 does not go brighter than white - it widens the part
// of the ribbon that is fully blown out, which is what gives the core its
// hard edge and the rim its softness.
static const float ARC_GAIN = 1.5f;

// The core line and the two edges it fades to. The edges are the SAME colour
// at zero alpha rather than black: additive blending never darkens, so an
// alpha of zero is already invisible and the colour only decides what the
// half-transparent middle ground looks like.
static const XMFLOAT4 ARC_CORE_COLOUR = XMFLOAT4(1.00f, 1.00f, 1.00f, 1.0f);
static const XMFLOAT4 ARC_RIM_COLOUR = XMFLOAT4(0.55f, 0.80f, 1.00f, 0.0f);

// Vertices per segment: two bands (inner->core and core->outer), two
// triangles each, three vertices a triangle.
static const int VERTS_PER_SEGMENT = 12;

// How many segments at the GROWING end of the wipe are faded out, so the edge
// the cut is being drawn towards comes to a point instead of stopping dead at
// full width. Without this the arc reads as a ribbon being extruded - the
// trailing tip is a proper point (the geometry tapers there) while the
// leading one is a clean vertical chop, and the chop is on the side the eye
// is following.
static const float ARC_LEAD_FADE = 8.0f;

// ---------------------------------------------------------------------------
// Shared GPU resources. The mesh is identical for every slash - only the
// world matrix and the alpha differ - so it is generated once and kept, the
// same way SlashEffect keeps its frames and ToonShader keeps its ramp.
// ---------------------------------------------------------------------------
static ID3D11Buffer* s_VertexBuffer = nullptr;
static ID3D11InputLayout* s_VertexLayout = nullptr;
static ID3D11VertexShader* s_VertexShader = nullptr;
static ID3D11PixelShader* s_PixelShader = nullptr;
static bool s_Loaded = false;

// The crescent as generated, kept on the CPU. The GPU buffer is written from
// this every draw with the leading-edge fade applied on top, so the shape
// itself is still only ever built once - what changes per frame is a handful
// of alphas, not the geometry.
static std::vector<VERTEX_3D> s_Template;

// Where each vertex sits along the arc, in segments (0 at the trailing tip,
// ARC_SEGMENTS at the leading one). PER VERTEX rather than per segment, and
// that distinction is the whole reason it is stored at all: a fade computed
// from the segment index is constant across each segment, so every segment
// boundary becomes a visible step and the soft edge turns into a row of
// stripes. Keyed to the vertex, the two segments either side of a boundary
// agree on the value there and the hardware interpolates straight through it.
static std::vector<float> s_ArcPos;

void SlashArc::LoadShared()
{
    if (s_Loaded)
        return;

    s_Loaded = true;

    const float half = ARC_SPREAD * 0.5f;
    const float sinHalf = sinf(half);
    const float cosHalf = cosf(half);

    // Guard against a degenerate spread being set above. A zero sinHalf would
    // divide every vertex by nothing and put the whole crescent on one point.
    const float norm = (sinHalf > 0.0001f) ? (1.0f / sinHalf) : 1.0f;

    std::vector<VERTEX_3D>& vertices = s_Template;
    vertices.clear();
    vertices.reserve(ARC_SEGMENTS * VERTS_PER_SEGMENT);

    // One point on the crescent's centre line, with the outward direction and
    // the ribbon's half width there.
    struct ArcPoint
    {
        XMFLOAT3 Inner;
        XMFLOAT3 Core;
        XMFLOAT3 Outer;
    };

    std::vector<ArcPoint> points;
    points.reserve(ARC_SEGMENTS + 1);

    for (int i = 0; i <= ARC_SEGMENTS; i++)
    {
        float u = i / (float)ARC_SEGMENTS;
        float theta = (u - 0.5f) * ARC_SPREAD;

        float sinT = sinf(theta);
        float cosT = cosf(theta);

        // The centre line, and the radial that is exactly its normal.
        float cx = sinT * norm;
        float cy = (cosT - cosHalf) * norm;

        float nx = sinT;
        float ny = cosT;

        // Zero at both ends, fattest in the middle. This is the taper - it is
        // what makes the ends points rather than cut-off rectangles.
        float width = ARC_HALF_WIDTH * powf(sinf(XM_PI * u), ARC_TIP_SHARPNESS);

        ArcPoint point;
        point.Inner = XMFLOAT3(cx - nx * width, cy - ny * width, 0.0f);
        point.Core = XMFLOAT3(cx, cy, 0.0f);
        point.Outer = XMFLOAT3(cx + nx * width, cy + ny * width, 0.0f);

        points.push_back(point);
    }

    // Emitted SEGMENT BY SEGMENT, both bands of a segment together, rather
    // than one whole band and then the other. That ordering is the whole
    // reason the wipe-on works: drawing the first N * VERTS_PER_SEGMENT
    // vertices gives N complete segments across the full width of the ribbon,
    // so the arc grows from its leading tip instead of half of it appearing.
    s_ArcPos.clear();
    s_ArcPos.reserve(ARC_SEGMENTS * VERTS_PER_SEGMENT);

    // ArcPos is which of the two ends of its segment this vertex came from,
    // in whole segments - so the shared edge between segment i and i + 1 gets
    // the same number from both sides.
    auto push = [&](const XMFLOAT3& Position, const XMFLOAT4& Colour, float ArcPos)
    {
        VERTEX_3D vertex;
        vertex.Position = Position;
        vertex.Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
        vertex.Diffuse = Colour;

        // Untextured - the pixel shader takes the else branch and writes the
        // interpolated vertex colour straight out, so these are never read.
        vertex.TexCoord = XMFLOAT2(0.0f, 0.0f);

        vertices.push_back(vertex);
        s_ArcPos.push_back(ArcPos);
    };

    for (int i = 0; i < ARC_SEGMENTS; i++)
    {
        const ArcPoint& a = points[i];
        const ArcPoint& b = points[i + 1];

        const float pa = (float)i;
        const float pb = (float)(i + 1);

        // Inner rim -> core.
        push(a.Inner, ARC_RIM_COLOUR, pa);
        push(b.Inner, ARC_RIM_COLOUR, pb);
        push(a.Core, ARC_CORE_COLOUR, pa);

        push(b.Inner, ARC_RIM_COLOUR, pb);
        push(b.Core, ARC_CORE_COLOUR, pb);
        push(a.Core, ARC_CORE_COLOUR, pa);

        // Core -> outer rim.
        push(a.Core, ARC_CORE_COLOUR, pa);
        push(b.Core, ARC_CORE_COLOUR, pb);
        push(a.Outer, ARC_RIM_COLOUR, pa);

        push(b.Core, ARC_CORE_COLOUR, pb);
        push(b.Outer, ARC_RIM_COLOUR, pb);
        push(a.Outer, ARC_RIM_COLOUR, pa);
    }

    // DYNAMIC, and shared by every live arc - the same arrangement
    // SlashEffect uses for its quad. Each instance writes the slice it is
    // about to draw immediately before drawing it, so two arcs on screen at
    // once cannot read each other's fade.
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = (UINT)(sizeof(VERTEX_3D) * vertices.size());
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertices.data();

    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &s_VertexBuffer);

    Renderer::CreateVertexShader(&s_VertexShader, &s_VertexLayout,
        "shader\\unlitTextureVS.cso");

    Renderer::CreatePixelShader(&s_PixelShader,
        "shader\\unlitTexturePS.cso");
}

void SlashArc::UninitShared()
{
    if (s_VertexBuffer) { s_VertexBuffer->Release(); s_VertexBuffer = nullptr; }
    if (s_VertexLayout) { s_VertexLayout->Release(); s_VertexLayout = nullptr; }
    if (s_VertexShader) { s_VertexShader->Release(); s_VertexShader = nullptr; }
    if (s_PixelShader) { s_PixelShader->Release();  s_PixelShader = nullptr; }

    s_Template.clear();
    s_Template.shrink_to_fit();

    s_ArcPos.clear();
    s_ArcPos.shrink_to_fit();

    s_Loaded = false;
}

void SlashArc::Init()
{
    m_Layer = 3; // the VFX layer - with Particle, Explosion and the trail

    LoadShared(); // no-op once the mesh is built
}

void SlashArc::Uninit()
{
    // Nothing per object - the mesh and the shaders are shared and outlive
    // every individual arc. Manager::Uninit frees them.
    GameObject::Uninit();
}

void SlashArc::Play(const Vector3& Position, float Angle, float Length, float Bow,
    float Lifetime, float Sweep)
{
    m_Position = Position;

    // Roll only. The crescent is generated in the play plane, so pitch and
    // yaw would tip it out of the plane the game happens on - and the old
    // sprite's pitch/yaw table existed only to stop a flat quad reading as a
    // sticker, which real geometry does not need.
    m_Rotation = Vector3(0.0f, 0.0f, Angle);

    m_Scale = Vector3(Length, Bow, 1.0f);

    m_BaseRoll = Angle;
    m_BaseLength = Length;
    m_BaseBow = Bow;

    m_Sweep = Sweep;

    if (Lifetime > 0.0f)
        m_Lifetime = Lifetime;

    m_Timer = 0.0f;
}

void SlashArc::Update()
{
    const float dt = 1.0f / 60.0f;

    m_Timer += dt;

    // Plays once and goes. Nothing holds a pointer to an arc, so there is no
    // way for one to be left standing in a scene.
    if (m_Timer >= m_Lifetime)
    {
        SetDestory();
        return;
    }

    GameObject::Update();
}

void SlashArc::Draw()
{
    if (s_VertexBuffer == nullptr)
        return;

    float t = m_Timer / m_Lifetime;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    // Shape and aim, both measured from what Play was handed rather than from
    // wherever the last frame left them.
    m_Scale.x = m_BaseLength * (ARC_LENGTH_START + (ARC_LENGTH_END - ARC_LENGTH_START) * t);
    m_Scale.y = m_BaseBow * (ARC_BOW_START + (ARC_BOW_END - ARC_BOW_START) * t);

    // Centred on the base angle, so the cut travels through the aim rather
    // than starting at it - half the sweep either side.
    m_Rotation.z = m_BaseRoll + m_Sweep * (t - 0.5f);

    // The wipe. The arc extends from its trailing tip over the first stretch
    // of its life and is whole after that.
    //
    // The edge is tracked as a FRACTIONAL position along the arc and only the
    // draw count is rounded up from it. Rounding the edge itself to whole
    // segments makes it jump a segment at a time, and at twenty-eight
    // segments across a wipe of a few frames that is a visible ratchet - the
    // fade below is anchored to this float, so the edge slides smoothly
    // between segments while the triangle count steps.
    float edge = (float)ARC_SEGMENTS;

    if (t < ARC_WIPE)
    {
        edge = ARC_SEGMENTS * (t / ARC_WIPE);

        if (edge < 0.0f)
            edge = 0.0f;
    }

    int segments = (int)ceilf(edge);

    if (segments < 1)
        segments = 1;
    if (segments > ARC_SEGMENTS)
        segments = ARC_SEGMENTS;

    float alpha = powf(1.0f - t, ARC_DECAY) * ARC_GAIN;

    Renderer::GetDeviceContext()->IASetInputLayout(s_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(s_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(s_PixelShader, NULL, 0);

    // The same three the rest of the VFX here set, for the same reasons:
    // additive so it reads as light, no depth test so it is never swallowed
    // by whatever it is cutting, no culling so no roll can turn it edge-on
    // and lose it. All three are put back at the end.
    Renderer::SetDepthEnable(false);
    Renderer::SetAddBlendEnable(true);
    Renderer::SetCullEnable(false);

    // A real world-space plane, built by the base class from position,
    // rotation and scale exactly like every other object here.
    Renderer::SetWorldMatrix(GetMatrx());

    // unlitTextureVS multiplies Material.Diffuse into the vertex colour, so
    // this alpha scales the whole ribbon - core and rim together - and the
    // per-vertex alpha keeps the soft edge while it does.
    MATERIAL material{};
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, alpha);
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.TextureEnable = false; // vertex colour only - there is no texture

    Renderer::SetMaterial(material);

    // Write just the slice about to be drawn. WRITE_DISCARD throws the whole
    // buffer away first, so writing a prefix and drawing exactly that prefix
    // is safe - nothing downstream ever reads the rest.
    const int used = segments * VERTS_PER_SEGMENT;

    D3D11_MAPPED_SUBRESOURCE msr{};
    if (FAILED(Renderer::GetDeviceContext()->Map(s_VertexBuffer, 0,
        D3D11_MAP_WRITE_DISCARD, 0, &msr)))
        return;

    VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;

    // Only mid-wipe is there an edge to soften. Once the arc is whole, both
    // its ends are the geometry's own tapered points and touching the alpha
    // would only dull them.
    const bool wiping = (edge < (float)ARC_SEGMENTS);

    for (int i = 0; i < used; i++)
    {
        vertex[i] = s_Template[i];

        if (!wiping)
            continue;

        // How far behind the growing edge this VERTEX is. A vertex at or past
        // the edge fades to nothing and the ones behind it ramp back up to
        // full, which is what turns the growing end into a point.
        float behind = edge - s_ArcPos[i];

        if (behind < 0.0f)
            behind = 0.0f;

        if (behind < ARC_LEAD_FADE)
            vertex[i].Diffuse.w *= behind / ARC_LEAD_FADE;
    }

    Renderer::GetDeviceContext()->Unmap(s_VertexBuffer, 0);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;

    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &s_VertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Renderer::GetDeviceContext()->Draw(used, 0);

    Renderer::SetCullEnable(true);
    Renderer::SetDepthEnable(true);
    Renderer::SetAddBlendEnable(false);
}
