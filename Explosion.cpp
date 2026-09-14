#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Explosion.h"
#include "Camera.h"

#define SPRITE_ROW (4)
#define SPRITE_COLUMNS (4)
#define NUM_SPRTIE (SPRITE_ROW * SPRITE_COLUMNS)

// The quad, in its own space: x -HALF_WIDTH..+HALF_WIDTH, y 0..HEIGHT, so it
// stands on its position.
static const float QUAD_HALF_WIDTH = 4.0f;
static const float QUAD_HEIGHT = 10.0f;

// Where the rows of Smoke.png actually are.
//
// The sheet is 1254x1254. Its four COLUMNS are an exact quarter each - a cut
// at every 1/4 crosses zero opaque pixels - but its four ROWS are not. Read
// off the sheet's own fully transparent gaps, the bands are 333, 329, 345 and
// 247 pixels tall, against the 313.5 an even quarter would give. Cutting the
// rows at a flat 1/4 therefore ran the line straight through the artwork: the
// three uniform cuts cross 28, 195 and 364 opaque pixels. That is why the
// bottom of one puff appeared along the top of the next frame.
//
// These are the gaps, as v coordinates. Every one of the 16 frames is whole
// inside them, and each boundary sits in the middle of a band at least 37
// pixels tall, so there is nothing for the sampler to bleed in either.
static const float SPRITE_V[SPRITE_ROW + 1] =
{
    0.000000f,   //    0
    0.265550f,   //  333
    0.527911f,   //  662
    0.803030f,   // 1007
    1.000000f,   // 1254
};


void Explosion::Init()
{
    m_Layer = 3;

    // The quad below spans x -4..+4 and y 0..10, so this scale is what
    // decides the burst's real size: 8 * scale wide, 10 * scale tall.
    //
    // It was 0.5 - a 4 x 5 unit burst standing over a 1.0 x 1.4 enemy, three
    // and a half times the height of the thing that just died, which is why
    // one kill filled the screen. 0.25 gives 2 x 2.5, a little bigger than
    // the enemy and still clearly a pop.
    m_Scale = { 0.25f, 0.25f, 0.25f };

    VERTEX_3D vertex[4];

    vertex[0].Position = XMFLOAT3(-4.0f, 10.0f, 0.0f);
    vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

    vertex[1].Position = XMFLOAT3(4.0f, 10.0f, 0.0f);
    vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

    vertex[2].Position = XMFLOAT3(-4.0f, 0.0f, 0.0f);
    vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

    vertex[3].Position = XMFLOAT3(4.0f, 0.0f, 0.0f);
    vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
    vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

    // 頂点バッファ生成
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(VERTEX_3D) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

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
    LoadFromWICFile(L"asset\\texture\\Smoke.png", WIC_FLAGS_NONE, &metadata, image);
    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &m_Texture);
    assert(m_Texture);
}
void Explosion::Uninit()
{
    m_VertexBuffer->Release();

    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();
}
void  Explosion::Update()
{
    m_Count++;

    if (m_Count % 3 == 0)
    {
        m_Frame++;
    }

    if (m_Frame >= NUM_SPRTIE)
    {
        SetDestory();
        return;
    }

    GameObject::Update();
}
void Explosion::Draw()
{
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    Camera* camera = Manager::GetGameObj<Camera>();

    XMMATRIX view = camera->GetViewMatrix();
    XMMATRIX invView = XMMatrixInverse(NULL, view);

    // Billboard
    invView.r[3].m128_f32[0] = 0.0f;
    invView.r[3].m128_f32[1] = 0.0f;
    invView.r[3].m128_f32[2] = 0.0f;

    XMMATRIX scale = XMMatrixScaling(
        m_Scale.x,
        m_Scale.y,
        m_Scale.z);

    XMMATRIX trans = XMMatrixTranslation(
        m_Position.x,
        m_Position.y,
        m_Position.z);

    XMMATRIX world = scale * invView * trans;

    Renderer::SetWorldMatrix(world);

    MATERIAL material{};
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.TextureEnable = true;

    Renderer::SetMaterial(material);

    Renderer::GetDeviceContext()->PSSetShaderResources(
        0,
        1,
        &m_Texture);

    // ==================================================
    // Sprite Sheet UV
    // ==================================================

    const int wc = SPRITE_COLUMNS;    // columns

    int column = m_Frame % wc;
    int row = m_Frame / wc;

    float w = 1.0f / (float)wc;
    float x = (float)column * w;

    // Rows come out of the table above, not from an even division.
    float v0 = SPRITE_V[row];
    float v1 = SPRITE_V[row + 1];

    // The rows are different heights, so a fixed quad would squash the short
    // ones and stretch the tall ones - trading the old artefact for a new
    // one. Scaling the quad by the row's share of the sheet keeps every frame
    // at the same pixels-per-unit. Measured against the even quarter a
    // uniform sheet would have had, so the middle rows stay close to the
    // original size and only the short last row shrinks.
    float rowScale = (v1 - v0) * (float)SPRITE_ROW;
    float quadTop = QUAD_HEIGHT * rowScale;

    D3D11_MAPPED_SUBRESOURCE msr{};

    if (SUCCEEDED(Renderer::GetDeviceContext()->Map(
        m_VertexBuffer,
        0,
        D3D11_MAP_WRITE_DISCARD,
        0,
        &msr)))
    {
        VERTEX_3D* v = (VERTEX_3D*)msr.pData;

        v[0].Position = XMFLOAT3(-QUAD_HALF_WIDTH, quadTop, 0.0f);
        v[0].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
        v[0].Diffuse = XMFLOAT4(1, 1, 1, 1);
        v[0].TexCoord = XMFLOAT2(x, v0);

        v[1].Position = XMFLOAT3(QUAD_HALF_WIDTH, quadTop, 0.0f);
        v[1].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
        v[1].Diffuse = XMFLOAT4(1, 1, 1, 1);
        v[1].TexCoord = XMFLOAT2(x + w, v0);

        v[2].Position = XMFLOAT3(-QUAD_HALF_WIDTH, 0.0f, 0.0f);
        v[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
        v[2].Diffuse = XMFLOAT4(1, 1, 1, 1);
        v[2].TexCoord = XMFLOAT2(x, v1);

        v[3].Position = XMFLOAT3(QUAD_HALF_WIDTH, 0.0f, 0.0f);
        v[3].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
        v[3].Diffuse = XMFLOAT4(1, 1, 1, 1);
        v[3].TexCoord = XMFLOAT2(x + w, v1);

        Renderer::GetDeviceContext()->Unmap(
            m_VertexBuffer,
            0);
    }

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;

    Renderer::GetDeviceContext()->IASetVertexBuffers(
        0,
        1,
        &m_VertexBuffer,
        &stride,
        &offset);

    Renderer::GetDeviceContext()->IASetPrimitiveTopology(
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    Renderer::GetDeviceContext()->Draw(4, 0);
}


