#include "main.h"
#include "renderer.h"
#include "Box.h"
#include "Explosion.h"
#include "animationModel.h"
#include "input.h"

#include "Manager.h"
#include "Camera.h"

#include "Enemy.h"

void Box::Init()
{
    m_Layer = 1;
    m_Position = { 0.0f, 0.0f, 0.0f };

    // ModelRenderer only parses Wavefront OBJ. AnimationModel is the one that
    // goes through assimp, so FBX props ride on that - the same as Sword does.
    AnimationModel* animationModel = AddGameComponent<AnimationModel>(this);
    animationModel->Load("asset\\model\\Box\\Box.fbx");

    // Collision::SolidFromBox treats a crate as spanning -1..1 in x and z and
    // 0..2 in y, then scaled by the object - a crate standing on its position.
    // Box.fbx is modelled around 100 times that and hangs below its origin, so
    // measure what was actually loaded and map it onto what the collision
    // expects. The stage scales are the collision half sizes too, so they
    // cannot absorb the difference themselves. Measured rather than hard
    // coded, so re-exporting the model at a different size does not silently
    // break the crates again.
    XMFLOAT3 boundsMin = animationModel->GetBoundsMin();
    XMFLOAT3 boundsMax = animationModel->GetBoundsMax();

    Vector3 size(boundsMax.x - boundsMin.x,
                 boundsMax.y - boundsMin.y,
                 boundsMax.z - boundsMin.z);

    m_FitScale.x = (size.x > 0.0001f) ? 2.0f / size.x : 1.0f;
    m_FitScale.y = (size.y > 0.0001f) ? 2.0f / size.y : 1.0f;
    m_FitScale.z = (size.z > 0.0001f) ? 2.0f / size.z : 1.0f;

    // Centre it on x and z, and stand it on y.
    m_FitOffset.x = -(boundsMin.x + boundsMax.x) * 0.5f * m_FitScale.x;
    m_FitOffset.y = -boundsMin.y * m_FitScale.y;
    m_FitOffset.z = -(boundsMin.z + boundsMax.z) * 0.5f * m_FitScale.z;

    // シェーダー読込
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\unlitTextureVS.cso");

    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\unlitTexturePS.cso");
}

void Box::Uninit()
{
    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();

    GameObject::Uninit();
}

void Box::Update()
{
    float dt = 1.0 / 60.0f;
    

    GameObject::Update();
}

void Box::Draw()
{
    // 入力レイアウト設定
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

    // シェーダ設定
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    // Same as GameObject::Draw, with the model fit from Init applied in front
    // of the object's own transform.
    XMMATRIX fit = XMMatrixScaling(m_FitScale.x, m_FitScale.y, m_FitScale.z)
        * XMMatrixTranslation(m_FitOffset.x, m_FitOffset.y, m_FitOffset.z);

    Renderer::SetWorldMatrix(fit * GetMatrx());

    for (Component* component : m_Components)
        component->Draw();
}
