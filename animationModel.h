#pragma once

#include <unordered_map>

#include "assimp/cimport.h"
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/matrix4x4.h"
#pragma comment (lib, "assimp-vc143-mt.lib")

#include "component.h"


//変形後頂点構造体
struct DEFORM_VERTEX
{
	aiVector3D Position;
	aiVector3D Normal;
	int				BoneNum;
	std::string		BoneName[4];//本来はボーンインデックスで管理するべき
	float			BoneWeight[4];
};

//ボーン構造体
struct BONE
{
	aiMatrix4x4 Matrix;
	aiMatrix4x4 AnimationMatrix;
	aiMatrix4x4 OffsetMatrix;
	aiMatrix4x4 WorldMatrix;
};

class AnimationModel : public Component
{
private:
	const aiScene* m_AiScene = nullptr;
	std::unordered_map<std::string, const aiScene*> m_Animation;

	ID3D11Buffer**	m_VertexBuffer;
	ID3D11Buffer**	m_IndexBuffer;

	std::unordered_map<std::string, ID3D11ShaderResourceView*> m_Texture;

	std::vector<DEFORM_VERTEX>* m_DeformVertex;//変形後頂点データ
	std::unordered_map<std::string, BONE> m_Bone;//ボーンデータ（名前で参照）

	// Node transform per mesh index, so a mesh that no bone ever moves can
	// still be placed the way its FBX node says (see Load).
	std::vector<aiMatrix4x4> m_MeshTransform;

	// Model space extents, so an owner can see how big what it loaded really
	// is instead of assuming the artist matched the engine's conventions.
	XMFLOAT3 m_BoundsMin{ 0.0f, 0.0f, 0.0f };
	XMFLOAT3 m_BoundsMax{ 0.0f, 0.0f, 0.0f };

	bool m_Flash = false;
	XMFLOAT4 m_FlashColor{ 1.0f, 1.0f, 1.0f, 1.0f };

	void CreateBone(aiNode* Node);
	void CollectMeshTransforms(aiNode* Node, const aiMatrix4x4& ParentMatrix);
	void UpdateBoneMatrix(aiNode* Node, aiMatrix4x4 Matrix);

public:
	using Component::Component;

	void Load( const char *FileName );

	XMFLOAT3 GetBoundsMin() const { return m_BoundsMin; }
	XMFLOAT3 GetBoundsMax() const { return m_BoundsMax; }

	// Flat colour override, the same pair ModelRenderer already has and
	// implemented the same way - Enemy drives both off one code path, so an
	// FBX enemy flashes white when hurt and pulses red on its wind-up exactly
	// like an OBJ one. Without this the tell simply would not appear on a
	// model that happens to be an FBX.
	void SetFlash(bool Flash) { m_Flash = Flash; }
	void SetFlashColor(const XMFLOAT4& Color) { m_FlashColor = Color; }
	void LoadAnimation( const char *FileName, const char *Name );
	void Uninit() override;
	void Update(const char* AnimationName1, int Frame1,
				const char* AnimationName2, int Frame2, float Blend);
	void Draw() override;
	bool GetBoneMatrix(const std::string& BoneName, XMMATRIX* OutMatrix) const;
	void DebugPrintBoneNames() const;

	// number of keyframes in this clip's first channel - this engine
	// treats "frame" as a raw keyframe index (see Update()), not a
	// time-sampled duration, so this is how long a one-shot animation
	// like an attack actually plays for before it should loop/end.
	int GetAnimationFrameCount(const std::string& AnimationName) const;

	// Every loaded clip: how many keys it has, what rate the FILE says it was
	// authored at, and how long that makes it.
	//
	// Worth printing once, because Update() takes an integer frame and indexes
	// the keys directly - it never looks at mTicksPerSecond. Playback is
	// therefore always one key per game frame. A clip authored at 30fps runs
	// at double speed, one at 24fps at two and a half times, and nothing in
	// the code says so.
	void DebugPrintAnimationInfo() const;
};