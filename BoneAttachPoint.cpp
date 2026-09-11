#include "main.h"
#include "BoneAttachPoint.h"
#include "renderer.h"
#include "GameObject.h"
#include "RotationUtil.h"

void BoneAttachPoint::SetBone(AnimationModel* Model, const char* BoneName)
{
    m_AnimationModel = Model;
    m_BoneName = BoneName;
}

void BoneAttachPoint::SetLocalTransform(const Vector3& Position, const Vector3& Rotation, const Vector3& Scale)
{
    m_LocalPosition = Position;
    m_LocalRotation = Rotation;
    m_LocalScale = Scale;
}

void BoneAttachPoint::Attach(GameObject* Item)
{
    if (Item == nullptr)
        return;

    m_Attached = Item;
    m_Attached->SetParent(m_GameObject);
}

GameObject* BoneAttachPoint::Detach()
{
    GameObject* item = m_Attached;
    if (item != nullptr)
    {
        item->SetParent(nullptr);
        item->ClearLocalMatrix(); // back to driving itself by position/rotation/scale
    }

    m_Attached = nullptr;
    return item;
}

void BoneAttachPoint::Update()
{
    if (m_Attached == nullptr || m_AnimationModel == nullptr)
        return;

    XMMATRIX boneMatrix;
    if (!m_AnimationModel->GetBoneMatrix(m_BoneName, &boneMatrix))
        return;

    XMMATRIX offset =
        XMMatrixScaling(m_LocalScale.x, m_LocalScale.y, m_LocalScale.z) *
        XMMatrixRotationRollPitchYaw(m_LocalRotation.x, m_LocalRotation.y, m_LocalRotation.z) *
        XMMatrixTranslation(m_LocalPosition.x, m_LocalPosition.y, m_LocalPosition.z);

    XMMATRIX finalMatrix = offset * boneMatrix;

    // The sword is parented to m_GameObject (e.g. Player), whose own
    // GetMatrx() will multiply this local transform by its own scale
    // (e.g. Player's 0.01 body-mesh correction). That scale has nothing
    // to do with the weapon's own mesh units, so cancel it back out of
    // just the scale component - position/rotation still inherit the
    // parent normally, which is what makes it "follow".
    XMVECTOR ownerScale, ownerRotQuat, ownerTranslation;
    if (XMMatrixDecompose(&ownerScale, &ownerRotQuat, &ownerTranslation, m_GameObject->GetMatrx()))
    {
        XMFLOAT3 ownerScaleF;
        XMStoreFloat3(&ownerScaleF, ownerScale);

        if (ownerScaleF.x != 0.0f && ownerScaleF.y != 0.0f && ownerScaleF.z != 0.0f)
        {
            finalMatrix = XMMatrixScaling(1.0f / ownerScaleF.x,
                1.0f / ownerScaleF.y,
                1.0f / ownerScaleF.z) * finalMatrix;
        }
    }

    // Hand the bone transform over as a matrix. Going through
    // position/rotation/scale here would mean packing the bone's rotation
    // into three Euler angles and rebuilding it in GameObject::GetMatrx()
    // - a round trip that loses the rotation as soon as the hand turns
    // around more than one axis (and flips near gimbal lock), which is
    // exactly when a sword swing looks broken.
    m_Attached->SetLocalMatrix(finalMatrix);

    // Position/rotation/scale are still kept in sync so gameplay code and
    // the debug print can read the weapon's transform - they no longer
    // drive the rendering, so an imperfect decompose can't distort it.
    XMVECTOR scale, rotQuat, translation;
    if (!XMMatrixDecompose(&scale, &rotQuat, &translation, finalMatrix))
        return;

    Vector3 position;
    XMStoreFloat3((XMFLOAT3*)&position, translation);

    Vector3 scaleVec;
    XMStoreFloat3((XMFLOAT3*)&scaleVec, scale);

    m_Attached->SetPosition(position);
    m_Attached->SetRotation(EulerFromQuaternion(rotQuat));
    m_Attached->SetScale(scaleVec);
}

void BoneAttachPoint::DebugPrintTransform() const
{
    char buffer[256];

    sprintf_s(buffer,
        "[Socket] offset pos(%.4f, %.4f, %.4f) rot(%.4f, %.4f, %.4f) scale(%.4f, %.4f, %.4f)\n",
        m_LocalPosition.x, m_LocalPosition.y, m_LocalPosition.z,
        m_LocalRotation.x, m_LocalRotation.y, m_LocalRotation.z,
        m_LocalScale.x, m_LocalScale.y, m_LocalScale.z);
    OutputDebugStringA(buffer);

    if (m_Attached != nullptr)
    {
        Vector3 pos = m_Attached->GetPosition();
        Vector3 rot = m_Attached->GetRotation();
        Vector3 scale = m_Attached->GetScale();

        sprintf_s(buffer,
            "[Sword] applied pos(%.4f, %.4f, %.4f) rot(%.4f, %.4f, %.4f) scale(%.4f, %.4f, %.4f)\n",
            pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, scale.x, scale.y, scale.z);
        OutputDebugStringA(buffer);
    }
    else
    {
        OutputDebugStringA("[Sword] nothing attached\n");
    }
}

void BoneAttachPoint::AdjustLocalPosition(const Vector3& Delta)
{
    m_LocalPosition += Delta;
}

void BoneAttachPoint::AdjustLocalRotation(const Vector3& Delta)
{
    m_LocalRotation += Delta;
}
