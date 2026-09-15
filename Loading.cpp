#include "main.h"
#include "renderer.h"
#include "Loading.h"

#include "manager.h"
#include "Font.h"
#include "Game.h"
#include "Stage.h"

#include <math.h>

// How long this screen is held before the map is asked for, and how many
// drawn frames it must have had. Both have to pass: the timer keeps the
// screen from flashing by too fast to read, and the frame count is what
// actually proves it was presented.
//
// Short on purpose. This replaces a three second wait on the title screen,
// so the game now starts sooner than it used to AND says something while it
// does - the delay was never the thing worth keeping.
static const float MINIMUM_HOLD = 0.45f;
static const int MINIMUM_FRAMES = 2;

static const XMFLOAT4 COLOUR_BACKDROP = XMFLOAT4(0.04f, 0.05f, 0.07f, 1.0f);
static const XMFLOAT4 COLOUR_RULE     = XMFLOAT4(0.82f, 0.62f, 0.30f, 0.85f);
static const XMFLOAT4 COLOUR_HEADING  = XMFLOAT4(1.00f, 0.97f, 0.90f, 1.0f);
static const XMFLOAT4 COLOUR_STAGE    = XMFLOAT4(0.72f, 0.80f, 0.76f, 1.0f);
static const XMFLOAT4 COLOUR_SHADOW   = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.65f);

static const float HEADING_Y = 300.0f;
static const float STAGE_Y = 380.0f;
static const float RULE_Y = 364.0f;
static const float RULE_WIDTH = 260.0f;
static const float RULE_HEIGHT = 2.0f;

void Loading::Init()
{
    m_Time = 0.0f;
    m_FramesDrawn = 0;
    m_Requested = false;

    // Flat quads only - no texture. Anything this screen had to read off
    // disk would be loading time of its own, which is the one thing a
    // loading screen cannot afford. The shaders are already compiled .cso
    // files the rest of the UI uses.
    VERTEX_3D vertex[4]{};

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = sizeof(VERTEX_3D) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertex; // initial data, overwritten every Draw()

    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\unlitTextureVS.cso");
    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\unlitTexturePS.cso");
}

void Loading::Uninit()
{
    if (m_VertexBuffer) { m_VertexBuffer->Release(); m_VertexBuffer = nullptr; }
    if (m_VertexLayout) { m_VertexLayout->Release(); m_VertexLayout = nullptr; }
    if (m_VertexShader) { m_VertexShader->Release(); m_VertexShader = nullptr; }
    if (m_PixelShader)  { m_PixelShader->Release();  m_PixelShader = nullptr; }
}

void Loading::Update()
{
    m_Time += 1.0f / 60.0f;

    if (m_Requested)
        return;

    if (m_Time < MINIMUM_HOLD || m_FramesDrawn < MINIMUM_FRAMES)
        return;

    m_Requested = true;

    // Zero, not a fade. Manager fires a zero-length change at the end of
    // this same Update, so Game::Init - and the freeze that comes with it -
    // begins with this screen already presented and still on the front
    // buffer. A fade here would only be more waiting on top of the load.
    Manager::ChangeScene<Game>(0.0f);
}

void Loading::DrawFlatQuad(float X, float Y, float Width, float Height, const XMFLOAT4& Color)
{
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    Renderer::SetWorldViewProjection2D();
    Renderer::SetWorldMatrix(XMMatrixIdentity());

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    MATERIAL material{};
    material.Diffuse = Color;
    material.TextureEnable = false;
    Renderer::SetMaterial(material);

    D3D11_MAPPED_SUBRESOURCE msr{};
    if (SUCCEEDED(Renderer::GetDeviceContext()->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
    {
        VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;

        vertex[0].Position = XMFLOAT3(X, Y, 0.0f);
        vertex[0].Normal = XMFLOAT3(0, 0, 0);
        vertex[0].Diffuse = XMFLOAT4(1, 1, 1, 1); // tinted through Material.Diffuse
        vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

        vertex[1].Position = XMFLOAT3(X + Width, Y, 0.0f);
        vertex[1].Normal = XMFLOAT3(0, 0, 0);
        vertex[1].Diffuse = XMFLOAT4(1, 1, 1, 1);
        vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

        vertex[2].Position = XMFLOAT3(X, Y + Height, 0.0f);
        vertex[2].Normal = XMFLOAT3(0, 0, 0);
        vertex[2].Diffuse = XMFLOAT4(1, 1, 1, 1);
        vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

        vertex[3].Position = XMFLOAT3(X + Width, Y + Height, 0.0f);
        vertex[3].Normal = XMFLOAT3(0, 0, 0);
        vertex[3].Diffuse = XMFLOAT4(1, 1, 1, 1);
        vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

        Renderer::GetDeviceContext()->Unmap(m_VertexBuffer, 0);
    }

    Renderer::GetDeviceContext()->Draw(4, 0);
}

void Loading::Draw()
{
    m_FramesDrawn++;

    const float centreX = SCREEN_WIDTH * 0.5f;

    // Quads first, then the text. Font::Draw binds its own state, so a quad
    // after it would come out with the font's pipeline.
    DrawFlatQuad(0.0f, 0.0f, (float)SCREEN_WIDTH, (float)SCREEN_HEIGHT, COLOUR_BACKDROP);
    DrawFlatQuad(centreX - RULE_WIDTH * 0.5f, RULE_Y, RULE_WIDTH, RULE_HEIGHT, COLOUR_RULE);

    if (!Font::IsReady())
        return;

    // Deliberately no sweeping bar or spinner. The load blocks the message
    // loop, so any animation would sit frozen for most of the time this
    // screen is up - and a frozen spinner reads as a crash, which is worse
    // than no spinner at all. The dots move only while there is a frame to
    // move them, and stop somewhere sensible when the load takes over.
    int dots = ((int)(m_Time * 4.0f)) % 4;

    char heading[16] = "LOADING";
    for (int i = 0; i < dots; i++)
        heading[7 + i] = '.';
    heading[7 + dots] = '\0';

    Font::DrawCentered(heading, centreX + 3.0f, HEADING_Y + 4.0f, 54.0f, COLOUR_SHADOW);
    Font::DrawCentered(heading, centreX, HEADING_Y, 54.0f, COLOUR_HEADING);

    // Which map is coming. Game::s_Stage is already pointing at it, and the
    // name comes from the same table the map is built from, so this cannot
    // disagree with what actually loads.
    const StageData& stage = GetStageData(Game::GetStageIndex());

    Font::DrawCentered(stage.Name, centreX + 2.0f, STAGE_Y + 2.0f, 24.0f, COLOUR_SHADOW);
    Font::DrawCentered(stage.Name, centreX, STAGE_Y, 24.0f, COLOUR_STAGE);
}
