#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Particle.h"
#include "Camera.h"
#include "input.h"

// The old firework's look, kept exactly as it was so calling Burst() gives
// the same effect it always gave.
static const float     BURST_SIZE   = 1.0f;
static const XMFLOAT4  BURST_COLOUR = XMFLOAT4(1.0f, 0.2f, 0.2f, 1.0f);

// Jump dust: small, pale and short lived. Additive blending is on, so this
// reads as a scuff of light rather than as a lump of dirt - which is the
// only kind of dust this renderer can draw without a second blend mode.
static const int   DUST_COUNT      = 9;
static const float DUST_SIZE       = 0.32f;
static const int   DUST_LIFE       = 22;    // frames, about a third of a second
static const float DUST_SIDE_SPEED = 2.6f;  // outward along x
static const float DUST_UP_SPEED   = 1.6f;
static const XMFLOAT4 DUST_COLOUR  = XMFLOAT4(0.80f, 0.78f, 0.70f, 1.0f);


void Particle::Init()
{
    m_Layer = 3;

    VERTEX_3D vertex[4];

    vertex[0].Position = XMFLOAT3(-0.5f, 0.5f, 0.0f);
    vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

    vertex[1].Position = XMFLOAT3(0.5f, 0.5f, 0.0f);
    vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

    vertex[2].Position = XMFLOAT3(-0.5f, -0.5f, 0.0f);
    vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

    vertex[3].Position = XMFLOAT3(0.5f, -0.5f, 0.0f);
    vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

    // 頂点バッファ生成
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(VERTEX_3D) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertex;

    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

    // シェーダー読込
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\unlitTextureVS.cso");

    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\unlitTexturePS.cso");

    TexMetadata metadata;
    ScratchImage image;
    LoadFromWICFile(L"asset\\bill_board\\particle.png", WIC_FLAGS_NONE, &metadata, image); 
    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &m_LaunchTexture);
    assert(m_LaunchTexture);

    LoadFromWICFile(L"asset\\bill_board\\tora.png", WIC_FLAGS_NONE, &metadata, image);
    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &m_ToraTexture);
    assert(m_ToraTexture);

    // Dust gets its own sprite. Without it the jump puff drew with the sheet
    // above - tora.png, which is a photograph of a face - because that is
    // what every non-LAUNCH particle fell through to.
    LoadFromWICFile(L"asset\\bill_board\\dust.png", WIC_FLAGS_NONE, &metadata, image);
    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &m_DustTexture);
    assert(m_DustTexture);

    

    for (int i = 0; i < PARTICLE_MAX; i++)
    {
        m_Particle[i].Enable = false;
        m_Particle[i].Size = BURST_SIZE;
        m_Particle[i].Colour = BURST_COLOUR;
    }
}
void Particle::Uninit()
{
    if (m_LaunchTexture)
        m_LaunchTexture->Release();

    if (m_ToraTexture)
        m_ToraTexture->Release();

    m_VertexBuffer->Release();

    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();

    // These were never released. One Particle object is built per stage, so
    // it leaked three textures per map - tora.png alone is 1.4MB.
    if (m_LaunchTexture) { m_LaunchTexture->Release(); m_LaunchTexture = nullptr; }
    if (m_ToraTexture)   { m_ToraTexture->Release();   m_ToraTexture = nullptr; }
    if (m_DustTexture)   { m_DustTexture->Release();   m_DustTexture = nullptr; }
}

Particle::PARTICLE* Particle::Spawn(const Vector3& Position, const Vector3& Velocity,
    int Life, float Size, const XMFLOAT4& Colour, PARTICLE_TYPE Type)
{
    for (int i = 0; i < PARTICLE_MAX; i++)
    {
        if (m_Particle[i].Enable)
            continue;

        m_Particle[i].Enable = true;
        m_Particle[i].Type = Type;
        m_Particle[i].Position = Position;
        m_Particle[i].Velocity = Velocity;
        m_Particle[i].Life = Life;
        m_Particle[i].Size = Size;
        m_Particle[i].Colour = Colour;

        return &m_Particle[i];
    }

    return nullptr; // pool full - drop it rather than cut something else off
}

void Particle::JumpDust(const Vector3& Position)
{
    // Fanned out to both sides rather than fired randomly: a jump kicks dust
    // sideways off the floor, and a symmetric spread reads as that instead of
    // as a small explosion under the player.
    for (int i = 0; i < DUST_COUNT; i++)
    {
        float t = (DUST_COUNT > 1) ? (i / (float)(DUST_COUNT - 1)) : 0.5f;
        float spread = (t * 2.0f - 1.0f);          // -1 .. +1 across the feet

        float jitter = ((float)rand() / RAND_MAX) * 0.4f + 0.8f;

        Vector3 velocity;
        velocity.x = spread * DUST_SIDE_SPEED * jitter;
        velocity.y = DUST_UP_SPEED * jitter * (1.0f - fabsf(spread) * 0.45f);
        velocity.z = 0.0f; // the play plane is XY, like everything else here

        Vector3 position = Position;
        position.x += spread * 0.22f;
        position.y += 0.08f; // just off the floor, not buried in it

        // Not LAUNCH - that type turns itself into a firework when it starts
        // falling - and not EXPLOSION either, which is what draws tora.png.
        Spawn(position, velocity, DUST_LIFE, DUST_SIZE, DUST_COLOUR,
            PARTICLE_DUST);
    }
}

void  Particle::Update()
{
    float dt = 1.0f / 60.0f;
 

    // The SPACE-triggered firework that used to live here is gone. It fired
    // from this emitter's own fixed spot whether or not the player actually
    // left the ground - including mid air, where the jump is refused - and
    // it is not what a jump looks like. Player::Update calls JumpDust on the
    // frame it really jumps; Burst() is still here for the firework.
    Vector3 gravity{ 0.0f, -9.8f, 0.0f };

    for (int i = 0; i < PARTICLE_MAX; i++)
    {
        if (!m_Particle[i].Enable)
            continue;

        m_Particle[i].Velocity += gravity * dt;
        m_Particle[i].Position += m_Particle[i].Velocity * dt;

        if (m_Particle[i].Type == PARTICLE_LAUNCH)
        {
            if (m_Particle[i].Velocity.y <= 0.0f)
            {
                CreateExplosion(m_Particle[i].Position);
                m_Particle[i].Enable = false;
                continue;
            }
        }

        m_Particle[i].Life--;

        if (m_Particle[i].Life <= 0)
        {
            m_Particle[i].Enable = false;
        }
    }


}
void  Particle::Draw()
{
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    Camera* camera = Manager::GetGameObj<Camera>();
    XMMATRIX view = camera->GetViewMatrix();
    XMMATRIX invView = XMMatrixInverse(NULL, view);
    invView.r[3].m128_f32[0] = 0.0f;
    invView.r[3].m128_f32[1] = 0.0f;
    invView.r[3].m128_f32[2] = 0.0f;


    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    Renderer::SetDepthEnable(false);
    Renderer::SetAddBlendEnable(true);

    MATERIAL material{};
    material.Ambient = { 1.0f, 1.0f, 1.0f, 1.0f };
    material.TextureEnable = true;

    for (int i = 0; i < PARTICLE_MAX; i++)
    {
        if (!m_Particle[i].Enable)
            continue;

        // Set per particle, not once for the whole pool - dust and fireworks
        // are on screen at the same time and are not the same colour.
        material.Diffuse = m_Particle[i].Colour;
        Renderer::SetMaterial(material);

        if (m_Particle[i].Type == PARTICLE_LAUNCH)
        {
            Renderer::GetDeviceContext()->
                PSSetShaderResources(0, 1, &m_LaunchTexture);
        }
        else if (m_Particle[i].Type == PARTICLE_DUST)
        {
            Renderer::GetDeviceContext()->
                PSSetShaderResources(0, 1, &m_DustTexture);
        }
        else
        {
            Renderer::GetDeviceContext()->
                PSSetShaderResources(0, 1, &m_ToraTexture);
        }

        XMMATRIX world, scale, trans;
        float size = m_Particle[i].Size;
        scale = XMMatrixScaling(m_Scale.x * size, m_Scale.y * size, m_Scale.z * size);
        trans = XMMatrixTranslation(m_Particle[i].Position.x, m_Particle[i].Position.y, m_Particle[i].Position.z);
        world = scale * invView * trans;

        Renderer::SetWorldMatrix(world);

        Renderer::GetDeviceContext()->Draw(4, 0);
    }

    Renderer::SetDepthEnable(true);
    Renderer::SetAddBlendEnable(false);
    
}

void Particle::CreateExplosion(Vector3 pos)
{
    for (int n = 0; n < 50; n++)
    {
        for (int i = 0; i < PARTICLE_MAX; i++)
        {
            if (!m_Particle[i].Enable)
            {
                float theta =
                    XM_2PI * ((float)rand() / RAND_MAX);

                float phi =
                    XM_PI * ((float)rand() / RAND_MAX);

                float speed =
                    8.0f + ((float)rand() / RAND_MAX) * 12.0f;

                Vector3 dir;

                dir.x = sinf(phi) * cosf(theta);
                dir.y = cosf(phi);
                dir.z = sinf(phi) * sinf(theta);

                m_Particle[i].Enable = true;
                m_Particle[i].Type = PARTICLE_EXPLOSION;

                m_Particle[i].Position = pos;
                m_Particle[i].Velocity = dir * speed;
                m_Particle[i].Life = 100;
                m_Particle[i].Size = BURST_SIZE;
                m_Particle[i].Colour = BURST_COLOUR;

                break;
            }
        }
    }
}
