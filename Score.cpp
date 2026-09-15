#include "main.h"
#include "renderer.h"
#include "Score.h"

#define SPRITE_ROW (5)
#define SPRITE_COLUMNS (5)
#define NUM_SPRTIE (SPRITE_ROW * SPRITE_COLUMNS)

// How big one digit is drawn, and the most that can be shown.
//
// It used to be 50, four digits wide, always drawn from m_Position to the
// RIGHT - a 200 pixel block that ran straight through the potion slots once
// those arrived under the stat bars. At 26 it is a readout rather than a
// scoreboard, and it is laid out LEFTWARDS from m_Position (see Draw), so
// m_Position is the top-RIGHT corner and the digits cannot grow into
// anything no matter how high the count gets.
static const float DIGIT_SIZE = 26.0f;
static const int MAX_DIGITS = 4;

int Score::s_RunTotal = 0;

void Score::Init()
{
    m_Layer = 4;
    VERTEX_3D vertex[4];

    vertex[0].Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
    vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

    vertex[1].Position = XMFLOAT3(50.0f, 0.0f, 0.0f);
    vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

    vertex[2].Position = XMFLOAT3(0.0f, 50.0f, 0.0f);
    vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

    vertex[3].Position = XMFLOAT3(50.0f, 50.0f, 0.0f);
    vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
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
    LoadFromWICFile(L"asset\\texture\\number.png", WIC_FLAGS_NONE, &metadata, image);
    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &m_Texture);
    assert(m_Texture);

    m_Score = 0;
}

void Score::Uninit()
{
    m_VertexBuffer->Release();

    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();
}

void Score::Update()
{

}

void Score::Draw()
{
    // Input Layout
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

    // Shader
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    // Projection
    Renderer::SetWorldViewProjection2D();

    // World Matrix
    XMMATRIX world, scale, rot, trans;
    scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
    trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    world = scale * rot * trans;

    Renderer::SetWorldMatrix(world);

    // Material
    MATERIAL material{};
    material.Diffuse = XMFLOAT4(1, 1, 1, 1);
    material.TextureEnable = true;
    Renderer::SetMaterial(material);

    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);

    const int wc = SPRITE_COLUMNS;
    const int hc = SPRITE_ROW;

    const float w = 1.0f / (float)wc;
    const float h = 1.0f / (float)hc;

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

    int number = m_Score;

    // Only the digits the number actually has. Four of them always drawn
    // meant a fresh stage read "0000", which looks like a placeholder that
    // was never finished rather than a score of nothing.
    int digits = 1;
    for (int n = number; n >= 10; n /= 10)
        digits++;

    if (digits > MAX_DIGITS)
        digits = MAX_DIGITS;

    for (int i = 0; i < digits; i++)
    {
        int digit = number % 10;
        number /= 10;

        float u = (digit % wc) * w;
        float v = (digit / wc) * h;

        D3D11_MAPPED_SUBRESOURCE msr{};

        if (SUCCEEDED(Renderer::GetDeviceContext()->Map(
            m_VertexBuffer,
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &msr)))
        {
            VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;

            // Laid out leftwards from the anchor: i = 0 is the least
            // significant digit and sits immediately left of m_Position, so
            // the block is right-aligned for free and a fifth digit would
            // grow away from the rest of the HUD rather than into it.
            float x = -(float)(i + 1) * DIGIT_SIZE;

            vertex[0].Position = XMFLOAT3(x, 0.0f, 0.0f);
            vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
            vertex[0].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[0].TexCoord = XMFLOAT2(u, v);

            vertex[1].Position = XMFLOAT3(x + DIGIT_SIZE, 0.0f, 0.0f);
            vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
            vertex[1].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[1].TexCoord = XMFLOAT2(u + w, v);

            vertex[2].Position = XMFLOAT3(x, DIGIT_SIZE, 0.0f);
            vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
            vertex[2].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[2].TexCoord = XMFLOAT2(u, v + h);

            vertex[3].Position = XMFLOAT3(x + DIGIT_SIZE, DIGIT_SIZE, 0.0f);
            vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
            vertex[3].Diffuse = XMFLOAT4(1, 1, 1, 1);
            vertex[3].TexCoord = XMFLOAT2(u + w, v + h);

            Renderer::GetDeviceContext()->Unmap(m_VertexBuffer, 0);
        }

        Renderer::GetDeviceContext()->Draw(4, 0);
    }
}

