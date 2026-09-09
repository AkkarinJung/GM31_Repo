#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Camera.h"
#include "Player.h"
#include "input.h"

void Camera::Init()
{
    m_Layer = 0;
    m_Target.x = 4.0f;                   // camera initially looks this far right of spawn
    m_Position = { 4.0f, 3.0f, -7.0f };  // match m_Target.x so there's no snap on frame 1
}
void Camera::Uninit()
{

}
void Camera::Update()
{
    Player* player = Manager::GetGameObj<Player>();
    Vector3 playerPos = player->GetPosition();

    float dt = 1.0f / 60.0f;
    float t = 0.1f;

    // Y and Z follow the player normally (smoothed both directions).
    m_Target.y = m_Target.y * (1.0f - t) + (playerPos.y + 2.0f) * t;
    m_Target.z = m_Target.z * (1.0f - t) + playerPos.z * t;

    // X only ever catches up when the player has moved far enough right
    // to need it - the camera never scrolls back left. Player starts
    // left-of-center and the camera stays put until they walk past it.

        m_Target.x = m_Target.x * (1.0f - t) + playerPos.x * t;

    m_Target += m_Shake * cosf(m_ShakeTime * 100.0f);
    m_ShakeTime += dt;
    m_Shake *= 0.9f;

    m_Position.x = m_Target.x;
    m_Position.y = m_Target.y + 1.0f;
    m_Position.z = m_Target.z - 7.0f;
}
void  Camera::Draw()
{
    // プロジェクションマトリクス
    XMMATRIX projection = XMMatrixPerspectiveFovLH(1.1f,
        (float)SCREEN_WIDTH / SCREEN_HEIGHT, 1.0f, 1000.0f);

    Renderer::SetProjectionMatrix(projection);

    // ビューマトリクス
    XMFLOAT3 up = XMFLOAT3(0.0f, 1.0f, 0.0f);
    m_ViewMatrix = XMMatrixLookAtLH(XMLoadFloat3((XMFLOAT3*)&m_Position),
        XMLoadFloat3((XMFLOAT3*)&m_Target),
        XMLoadFloat3(&up));

    Renderer::SetViewMatrix(m_ViewMatrix);
}