#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "ImpactEffect.h"

// The same soft white blob the trail uses. Drawn square it is a round flash;
// stretched on one axis it is a streak. Two shapes out of one 256x256 png
// that the project already ships, which is the whole reason this needs no
// new art to work.
#define IMPACT_TEXTURE  L"asset\\texture\\trail.png"

// ---------------------------------------------------------------------------
// The burst, over its life (t goes 0 -> 1).
//
// Everything here is fast. An impact is a punctuation mark: it has to be over
// before the player's eye has finished moving to it, or it stops reading as a
// hit and starts reading as an explosion.
// ---------------------------------------------------------------------------

// ImpactDuration. The heavy variant gets a little longer to land.
static const float IMPACT_LIFE = 0.20f;
static const float IMPACT_LIFE_HEAVY = 0.26f;

// The flash. Snaps open over the first few percent of the life and then
// collapses - the collapse is what gives it a direction in time, where a
// flash that simply faded at constant size would just look like a light being
// switched off.
static const float FLASH_SIZE = 0.62f;   // half-extents at full size
static const float FLASH_OPEN = 0.12f;   // fraction of life spent opening
static const float FLASH_END_SCALE = 0.35f;   // size left at t=1
static const float FLASH_DECAY = 2.2f;    // higher = dies faster

// The two cut lines crossing the contact point. Long, very thin, and thrown
// out from the centre - they are what makes the burst read as something
// SLICED rather than something that exploded.
static const int   LINE_COUNT = 2;
static const float LINE_SPREAD = 0.62f;   // radians either side of the blow
static const float LINE_LENGTH = 0.70f;   // half-extents at t=1, so 1.4
                                          // world units end to end - shorter
                                          // than an enemy is tall, which is
                                          // what keeps them reading as cut
                                          // marks rather than as a second
                                          // slash
static const float LINE_LENGTH_START = 0.30f;
static const float LINE_WIDTH = 0.085f;
static const float LINE_DECAY = 2.6f;

// The spark fan. Thrown along the blow and slowed hard, so they are almost
// stopped by the time they fade - sparks that keep their speed to the end
// look like they were fired rather than knocked loose.
static const int   SPARK_COUNT = 6;
static const int   SPARK_COUNT_HEAVY = 8;
static const float SPARK_FAN = 1.15f;   // radians either side of the blow
static const float SPARK_SPEED = 7.0f;
static const float SPARK_SPEED_VARY = 0.45f;  // +/- this fraction
static const float SPARK_DRAG = 3.4f;   // how hard they slow down
static const float SPARK_LENGTH = 0.30f;
static const float SPARK_WIDTH = 0.055f;
static const float SPARK_DECAY = 1.7f;

// Everything is additive, so above 1 widens the blown-out core rather than
// making anything brighter than white.
static const float IMPACT_GAIN = 1.6f;
static const float HEAVY_SCALE = 1.45f;

// Warm white centre, cooler edges - the same two-colour trick the trail uses.
static const XMFLOAT4 FLASH_COLOUR = XMFLOAT4(1.00f, 0.97f, 0.86f, 1.0f);
static const XMFLOAT4 LINE_COLOUR = XMFLOAT4(1.00f, 1.00f, 1.00f, 1.0f);
static const XMFLOAT4 SPARK_COLOUR = XMFLOAT4(1.00f, 0.86f, 0.55f, 1.0f);

// ---------------------------------------------------------------------------
// Shared GPU resources.
//
// A hit can land on several enemies in one frame and a combo lands three
// times a second, so loading a texture and compiling two shaders per burst -
// which is what Explosion and DamageNumber do - is not affordable here. Same
// shape as SlashEffect's shared frames: loaded once, freed by Manager::Uninit.
// ---------------------------------------------------------------------------
static ID3D11Buffer* s_VertexBuffer = nullptr;
static ID3D11InputLayout* s_VertexLayout = nullptr;
static ID3D11VertexShader* s_VertexShader = nullptr;
static ID3D11PixelShader* s_PixelShader = nullptr;
static ID3D11ShaderResourceView* s_Texture = nullptr;
static bool s_Loaded = false;

void ImpactEffect::LoadShared()
{
    if (s_Loaded)
        return;

    s_Loaded = true;

    // A unit quad centred on the origin, so a world matrix places the middle
    // of a piece rather than its corner. Static: unlike SlashEffect, nothing
    // here animates UVs, so the buffer is written once and never touched
    // again - every piece is the same quad under a different matrix.
    VERTEX_3D vertex[4];

    vertex[0].Position = XMFLOAT3(-1.0f, 1.0f, 0.0f);
    vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);
    vertex[1].Position = XMFLOAT3(1.0f, 1.0f, 0.0f);
    vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);
    vertex[2].Position = XMFLOAT3(-1.0f, -1.0f, 0.0f);
    vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);
    vertex[3].Position = XMFLOAT3(1.0f, -1.0f, 0.0f);
    vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

    for (int i = 0; i < 4; i++)
    {
        vertex[i].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
        vertex[i].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DEFAULT;
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

    if (FAILED(LoadFromWICFile(IMPACT_TEXTURE, WIC_FLAGS_NONE, &metadata, image)))
    {
        // Not fatal - Draw checks and skips. A missing png costs the burst,
        // never the hit.
        OutputDebugStringA("ImpactEffect: could not load the impact texture\n");
        return;
    }

    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &s_Texture);
}

void ImpactEffect::UninitShared()
{
    if (s_Texture) { s_Texture->Release();      s_Texture = nullptr; }
    if (s_VertexBuffer) { s_VertexBuffer->Release(); s_VertexBuffer = nullptr; }
    if (s_VertexLayout) { s_VertexLayout->Release(); s_VertexLayout = nullptr; }
    if (s_VertexShader) { s_VertexShader->Release(); s_VertexShader = nullptr; }
    if (s_PixelShader) { s_PixelShader->Release();  s_PixelShader = nullptr; }

    s_Loaded = false;
}

void ImpactEffect::Init()
{
    m_Layer = 3; // the VFX layer, with Particle, Explosion and the slash

    LoadShared(); // no-op once it is in
}

void ImpactEffect::Uninit()
{
    // Nothing per object - the quad, the shaders and the texture are shared
    // and outlive every burst. Manager::Uninit frees them.
    GameObject::Uninit();
}

void ImpactEffect::Burst(const Vector3& Position, const Vector3& Direction,
    float Scale, bool Heavy)
{
    m_Position = Position;
    m_Heavy = Heavy;
    m_Scale = Scale * (Heavy ? HEAVY_SCALE : 1.0f);
    m_Lifetime = Heavy ? IMPACT_LIFE_HEAVY : IMPACT_LIFE;
    m_Timer = 0.0f;

    // Flattened onto the play plane before the angle is taken: the swing has
    // a little vertical reach and the burst is drawn on XY, so a direction
    // with height in it would tilt the whole fan out of the plane it is
    // drawn on.
    float dx = Direction.x;
    float dy = Direction.y;

    // A direction of zero length has no angle. Defaulting to +X rather than
    // letting atan2f(0, 0) answer 0 is the same thing numerically, but it
    // says that the fallback is deliberate.
    if (dx * dx + dy * dy < 0.000001f)
    {
        dx = 1.0f;
        dy = 0.0f;
    }

    m_Angle = atan2f(dy, dx);

    // The fan is rolled once, here, and then only replayed by Draw - so the
    // sparks do not re-randomise every frame, which would boil rather than
    // fly.
    m_SparkCount = Heavy ? SPARK_COUNT_HEAVY : SPARK_COUNT;
    if (m_SparkCount > SPARK_MAX)
        m_SparkCount = SPARK_MAX;

    for (int i = 0; i < m_SparkCount; i++)
    {
        // Spread evenly across the fan and then jittered, rather than placed
        // at random: pure random leaves gaps and clumps, and a clump of three
        // sparks on one side reads as the hit having a direction it does not.
        float spread = (m_SparkCount > 1)
            ? ((i / (float)(m_SparkCount - 1)) * 2.0f - 1.0f)
            : 0.0f;

        float jitter = ((float)rand() / RAND_MAX) * 0.36f - 0.18f;

        m_Spark[i].Angle = m_Angle + spread * SPARK_FAN + jitter;

        float vary = 1.0f + (((float)rand() / RAND_MAX) * 2.0f - 1.0f) * SPARK_SPEED_VARY;

        m_Spark[i].Speed = SPARK_SPEED * vary * m_Scale;
        m_Spark[i].Length = SPARK_LENGTH * vary * m_Scale;
        m_Spark[i].Width = SPARK_WIDTH * m_Scale;
    }
}

void ImpactEffect::Update()
{
    const float dt = 1.0f / 60.0f;

    m_Timer += dt;

    // Self-destructs the instant it is spent. Nothing holds a pointer to a
    // burst, so there is no way for one to be left behind in a scene.
    if (m_Timer >= m_Lifetime)
    {
        SetDestory();
        return;
    }

    GameObject::Update();
}

// One piece of the burst. Everything is a unit quad scaled, rolled about Z
// and dropped into the world - built here rather than through GetMatrx()
// because the pieces move independently of the object's own transform.
static void DrawPiece(const Vector3& Position, float Angle,
    float HalfLength, float HalfWidth,
    const XMFLOAT4& Colour, float Alpha)
{
    if (Alpha <= 0.0f || HalfLength <= 0.0f || HalfWidth <= 0.0f)
        return;

    XMMATRIX world =
        XMMatrixScaling(HalfLength, HalfWidth, 1.0f) *
        XMMatrixRotationZ(Angle) *
        XMMatrixTranslation(Position.x, Position.y, Position.z);

    Renderer::SetWorldMatrix(world);

    MATERIAL material{};
    material.Diffuse = XMFLOAT4(Colour.x, Colour.y, Colour.z, Alpha);
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.TextureEnable = true;

    Renderer::SetMaterial(material);

    Renderer::GetDeviceContext()->Draw(4, 0);
}

void ImpactEffect::Draw()
{
    if (s_VertexBuffer == nullptr || s_Texture == nullptr)
        return;

    float t = m_Timer / m_Lifetime;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    Renderer::GetDeviceContext()->IASetInputLayout(s_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(s_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(s_PixelShader, NULL, 0);

    // Additive light, drawn over whatever it landed on, and never culled
    // however the pieces end up rolled. The same three the rest of the VFX
    // here set, and put back below.
    Renderer::SetDepthEnable(false);
    Renderer::SetAddBlendEnable(true);
    Renderer::SetCullEnable(false);

    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &s_Texture);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;

    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &s_VertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // ---- the flash ------------------------------------------------------
    {
        float open = (t < FLASH_OPEN) ? (t / FLASH_OPEN) : 1.0f;
        float shrink = 1.0f + (FLASH_END_SCALE - 1.0f) * t;

        float size = FLASH_SIZE * m_Scale * open * shrink;
        float alpha = powf(1.0f - t, FLASH_DECAY) * IMPACT_GAIN;

        // Square, so the blob draws as the round core it is. No roll: a
        // circle has no orientation to get wrong.
        DrawPiece(m_Position, 0.0f, size, size, FLASH_COLOUR, alpha);
    }

    // ---- the cut lines --------------------------------------------------
    {
        float grow = LINE_LENGTH_START + (1.0f - LINE_LENGTH_START) * t;
        float alpha = powf(1.0f - t, LINE_DECAY) * IMPACT_GAIN;

        // Thinning as they stretch. Length x width roughly constant is what
        // makes a streak read as one thing being pulled rather than as a
        // rectangle being resized.
        float length = LINE_LENGTH * m_Scale * grow;
        float width = LINE_WIDTH * m_Scale * (1.0f - t * 0.75f);

        for (int i = 0; i < LINE_COUNT; i++)
        {
            // Straddling the blow rather than along it: two cuts crossing
            // near the contact point, which is what an impact looks like in
            // this genre. Both are measured off m_Angle, so they turn with
            // the swing.
            float side = (i == 0) ? 1.0f : -1.0f;

            DrawPiece(m_Position, m_Angle + side * LINE_SPREAD,
                length, width, LINE_COLOUR, alpha);
        }
    }

    // ---- the sparks -----------------------------------------------------
    {
        float alpha = powf(1.0f - t, SPARK_DECAY) * IMPACT_GAIN;

        // Position under linear drag, integrated rather than stepped, so a
        // spark is a pure function of t - nothing accumulates and a burst
        // caught by the hit-stop freeze cannot drift.
        float travel = (1.0f - expf(-SPARK_DRAG * m_Timer)) / SPARK_DRAG;

        for (int i = 0; i < m_SparkCount; i++)
        {
            const Spark& spark = m_Spark[i];

            float distance = spark.Speed * travel;

            Vector3 position = m_Position;
            position.x += cosf(spark.Angle) * distance;
            position.y += sinf(spark.Angle) * distance;

            // Shrinking as it slows. A spark that keeps its size to the end
            // pops out of existence; one that shrinks into it goes out.
            float shrink = 1.0f - t * 0.65f;

            // Rolled to its own direction of travel, so every streak points
            // the way it is going.
            DrawPiece(position, spark.Angle,
                spark.Length * shrink, spark.Width * shrink,
                SPARK_COLOUR, alpha);
        }
    }

    Renderer::SetCullEnable(true);
    Renderer::SetDepthEnable(true);
    Renderer::SetAddBlendEnable(false);
}
