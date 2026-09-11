#pragma once
#include "Vector3.h"
#include "component.h"

class GameObject
{
protected:
	bool m_Destroy = false;
	int m_Layer{ 1 };
	float m_CameraZ;

	Vector3 m_Position{ 0.0f, 0.0f, 0.0f };
	Vector3 m_Rotation{ 0.0f, 0.0f, 0.0f };
	Vector3 m_Scale{ 1.0f, 1.0f, 1.0f };

	std::list<Component*> m_Components;
	GameObject* m_Parent = nullptr;

	// Optional exact local matrix, used instead of position/rotation/scale
	// when something drives this object with a matrix (see SetLocalMatrix).
	// Stored as XMFLOAT4X4, not XMMATRIX: XMMATRIX needs 16 byte alignment,
	// which a heap allocated GameObject does not get on the Win32 build.
	bool m_UseLocalMatrix = false;
	XMFLOAT4X4 m_LocalMatrix;
public:
	const int GetLayer() const { return m_Layer; }

	float GetCameraZ() const { return m_CameraZ; }
	void CalcCameraZ(Vector3 CamPos, Vector3 CamForward)
	{
		Vector3 dir = m_Position - CamPos;
		m_CameraZ = Vector3::dot(dir, CamForward);
	}

	void SetPosition(const Vector3& Position) { m_Position = Position; }
	const Vector3& GetPosition() const { return m_Position; }

	void SetRotation(const Vector3& Rotation) { m_Rotation = Rotation; }
	const Vector3& GetRotation() const { return m_Rotation; }

	void SetScale(const Vector3& Scale) { m_Scale = Scale; }
	const Vector3& GetScale() const { return m_Scale; }

	void SetParent(GameObject* Parent) { m_Parent = Parent; }

	// Drive this object with an exact local matrix instead of
	// position/rotation/scale. Euler angles cannot express every matrix
	// (and never survive a decompose/recompose round trip cleanly), so
	// anything following an animated bone should hand over the matrix
	// itself - see BoneAttachPoint. The parent transform still applies.
	void SetLocalMatrix(const XMMATRIX& Matrix)
	{
		XMStoreFloat4x4(&m_LocalMatrix, Matrix);
		m_UseLocalMatrix = true;
	}
	void ClearLocalMatrix() { m_UseLocalMatrix = false; }
	bool HasLocalMatrix() const { return m_UseLocalMatrix; }

	XMMATRIX GetMatrx()
	{
		XMMATRIX world;

		if (m_UseLocalMatrix)
		{
			world = XMLoadFloat4x4(&m_LocalMatrix);
		}
		else
		{
			XMMATRIX scale, rot, trans;
			scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
			rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
			trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
			world = scale * rot * trans;
		}

		if (m_Parent)
		{
			world *= m_Parent->GetMatrx();
		}

		return world;
	}

	void SetDestory() { m_Destroy = true; }
	bool Destory() 
	{
		if (m_Destroy)
		{
			Uninit();
			delete this;
			return true;
		}
		else
		{
			return false;
		}
	}
	

	virtual void Init() {};
	virtual void Uninit() 
	{
		for (Component* component : m_Components)
		{
			component->Uninit();
			delete component;
		}
	};
	virtual void Update() 
	{
		for (Component* component : m_Components)
		{
			component->Update();
		}
	};
	virtual void Draw() 
	{
		// マトリクス設定
		XMMATRIX world = GetMatrx();
		Renderer::SetWorldMatrix(world);

		for (Component* component : m_Components)
		{
			component->Draw();
		}
	};

	template<typename T>
	T* AddGameComponent(GameObject* Object)
	{
		T* component = new T(Object);
		component->Init();
		m_Components.push_back(component);

		return component;
	}

	template<typename T>
	T* GetGameComponent()
	{
		for (Component* component : m_Components)
		{
			T* find = dynamic_cast<T*>(component);
			if (find != nullptr)
				return find;
		}
		return nullptr;
	}

	virtual Vector3 GetFoward()
	{
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);

		Vector3 forward;
		XMStoreFloat3((XMFLOAT3*)&forward, rot.r[2]);
		return forward;
	}

	virtual Vector3 GetRight()
	{
		XMMATRIX rot = XMMatrixRotationRollPitchYaw(
			m_Rotation.x,
			m_Rotation.y,
			m_Rotation.z);

		Vector3 right;
		XMStoreFloat3((XMFLOAT3*)&right, rot.r[0]);
		return right;
	}

};