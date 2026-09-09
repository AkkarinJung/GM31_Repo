#include "main.h"
#include "BoneAttachPoint.h"
#include "renderer.h"
#include "GameObject.h"


static Vector3 QuaternionToEuler(XMVECTOR Quat)
{
    XMFLOAT4 q;
    XMStoreFloat4(&q, Quat);

    float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    float roll = atan2f(sinr_cosp, cosr_cosp);

    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    float pitch = fabsf(sinp) >= 1.0f ? copysignf(XM_PIDIV2, sinp) : asinf(sinp);

    float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    float yaw = atan2f(siny_cosp, cosy_cosp);

    // GameObject uses XMMatrixRotationRollPitchYaw(x=pitch, y=yaw, z=roll)
    return { pitch, yaw, roll };
}

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
        item->SetParent(nullptr);

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

    XMVECTOR scale, rotQuat, translation;
    XMMatrixDecompose(&scale, &rotQuat, &translation, finalMatrix);

    Vector3 position;
    XMStoreFloat3((XMFLOAT3*)&position, translation);

    // The attached item stays parented to the SAME GameObject that owns
    // this AnimationModel (e.g. Player). Its own GetMatrx() will multiply
    // by that owner's world matrix on top of this, exactly like the
    // skinned mesh vertices already do - so this is correct without any
    // extra world-matrix math here.
    m_Attached->SetPosition(position);
    m_Attached->SetRotation(QuaternionToEuler(rotQuat));
}
