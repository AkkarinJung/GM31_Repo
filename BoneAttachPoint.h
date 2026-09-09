#pragma once
#include "component.h"
#include "Vector3.h"
#include "animationModel.h"

// Like AttachPoint, but follows a specific bone of the owner's
// AnimationModel instead of the owner's root transform - use this when
// the weapon must track the hand through the animation.
class BoneAttachPoint : public Component
{
private:
    AnimationModel* m_AnimationModel = nullptr;
    std::string m_BoneName;

    GameObject* m_Attached = nullptr;

    Vector3 m_LocalPosition{ 0.0f, 0.0f, 0.0f };
    Vector3 m_LocalRotation{ 0.0f, 0.0f, 0.0f };
    Vector3 m_LocalScale{ 1.0f, 1.0f, 1.0f };

public:
    using Component::Component;

    void Update() override;

    void SetBone(AnimationModel* Model, const char* BoneName);
    void SetLocalTransform(const Vector3& Position, const Vector3& Rotation,
        const Vector3& Scale = { 1.0f, 1.0f, 1.0f });

    void Attach(GameObject* Item);
    GameObject* Detach();

    GameObject* GetAttached() const { return m_Attached; }
};