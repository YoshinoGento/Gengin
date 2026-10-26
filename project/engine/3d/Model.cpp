#include "3d/Model.h"
#include "3d/ModelManager.h"
#include "base/LoadTiming.h"
#include "3d/Object3dCommon.h"
#include "3d/Skeleton.h"
#include "base/SrvManager.h"
#include "2d/TextureManager.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <limits>

/**
 * Assimpの行列(aiMatrix4x4)を独自の行列(Matrix4x4)に変換する
 * 資料スライド 9 に基づき、転置を行ってコピーします
 */
static Matrix4x4 ConvertAiMatrix(const aiMatrix4x4& aiMat) {
	Matrix4x4 result;
	// 要素のコピー
	result.m[0][0] = aiMat.a1; result.m[0][1] = aiMat.a2; result.m[0][2] = aiMat.a3; result.m[0][3] = aiMat.a4;
	result.m[1][0] = aiMat.b1; result.m[1][1] = aiMat.b2; result.m[1][2] = aiMat.b3; result.m[1][3] = aiMat.b4;
	result.m[2][0] = aiMat.c1; result.m[2][1] = aiMat.c2; result.m[2][2] = aiMat.c3; result.m[2][3] = aiMat.c4;
	result.m[3][0] = aiMat.d1; result.m[3][1] = aiMat.d2; result.m[3][2] = aiMat.d3; result.m[3][3] = aiMat.d4;

	// 資料の指示通りに転置(行列の向きを調整)
	result = MatrixMath::Transpose(result);

	Matrix4x4 flipX = MatrixMath::MakeIdentity4x4();
	flipX.m[0][0] = -1.0f;
	return MatrixMath::Multiply(flipX, MatrixMath::Multiply(result, flipX));
}

static bool IsFiniteMatrix(const Matrix4x4& matrix) {
	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			if (!std::isfinite(matrix.m[row][column])) {
				return false;
			}
		}
	}
	return true;
}

static void TransitionResource(
	ID3D12GraphicsCommandList* commandList,
	ID3D12Resource* resource,
	D3D12_RESOURCE_STATES before,
	D3D12_RESOURCE_STATES after) {
	assert(commandList);
	assert(resource);
	if (before == after) {
		return;
	}

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = resource;
	barrier.Transition.StateBefore = before;
	barrier.Transition.StateAfter = after;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);
}

Model::SkinCluster::~SkinCluster() {
	if (influenceResource && mappedInfluence) {
		influenceResource->Unmap(0, nullptr);
		mappedInfluence = nullptr;
	}
	if (paletteResource && mappedPalette) {
		paletteResource->Unmap(0, nullptr);
		mappedPalette = nullptr;
	}
	if (skinningInfoResource && mappedSkinningInfo) {
		skinningInfoResource->Unmap(0, nullptr);
		mappedSkinningInfo = nullptr;
	}
}

void Model::Initialize(ModelManager* modelManager, const std::string& directoryPath, const std::string& filename) {
	modelManager_ = modelManager;
	DirectXCommon* dxCommon = modelManager_->GetDxCommon();

	LoadModelFile(directoryPath, filename);
	loading::Scope bufferTiming(loading::Stage::ModelBuffers);

	vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertices_.size());
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = (UINT)(sizeof(VertexData) * vertices_.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	VertexData* vData = nullptr;
	vertexResource_->Map(0, nullptr, (void**)&vData);
	std::memcpy(vData, vertices_.data(), sizeof(VertexData) * vertices_.size());
	vertexResource_->Unmap(0, nullptr);

	if (!indices_.empty()) {
		indexCount_ = (UINT)indices_.size();
		indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * indices_.size());
		indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
		indexBufferView_.SizeInBytes = (UINT)(sizeof(uint32_t) * indices_.size());
		indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
		uint32_t* iData = nullptr;
		indexResource_->Map(0, nullptr, (void**)&iData);
		std::memcpy(iData, indices_.data(), sizeof(uint32_t) * indices_.size());
		indexResource_->Unmap(0, nullptr);
	}

	CreateSkinCluster();
}

/**
 * メモリ上のデータから直接初期化 (PrimitiveGenerator 用)
 */
void Model::InitializeWithData(ModelManager* modelManager, const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices) {
	modelManager_ = modelManager;
	vertices_ = vertices;
	indices_ = indices;

	// 生成モデルには階層がないため、RootNode を単位行列で初期化
	rootNode_.localMatrix = MatrixMath::MakeIdentity4x4();
	rootNode_.name = "GeneratedRoot";

	DirectXCommon* dxCommon = modelManager_->GetDxCommon();
	vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * vertices_.size());
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = (UINT)(sizeof(VertexData) * vertices_.size());
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	VertexData* vData = nullptr;
	vertexResource_->Map(0, nullptr, (void**)&vData);
	std::memcpy(vData, vertices_.data(), sizeof(VertexData) * vertices_.size());
	vertexResource_->Unmap(0, nullptr);

	if (!indices_.empty()) {
		indexCount_ = (UINT)indices_.size();
		indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * indices_.size());
		indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
		indexBufferView_.SizeInBytes = (UINT)(sizeof(uint32_t) * indices_.size());
		indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
		uint32_t* iData = nullptr;
		indexResource_->Map(0, nullptr, (void**)&iData);
		std::memcpy(iData, indices_.data(), sizeof(uint32_t) * indices_.size());
		indexResource_->Unmap(0, nullptr);
	}
}

Animation Model::LoadAnimation(const std::string& directoryPath, const std::string& filename) {
	Assimp::Importer importer;
	std::string filepath = directoryPath + "/" + filename;
	const aiScene* scene = importer.ReadFile(filepath.c_str(), 0);
	assert(scene && scene->mNumAnimations > 0);

	aiAnimation* animAssimp = scene->mAnimations[0];
	Animation animation;
	animation.duration = (float)animAssimp->mDuration / (float)animAssimp->mTicksPerSecond;

	for (uint32_t i = 0; i < animAssimp->mNumChannels; ++i) {
		aiNodeAnim* nodeAnimAssimp = animAssimp->mChannels[i];
		NodeAnimation& nodeAnim = animation.nodeAnimations[nodeAnimAssimp->mNodeName.C_Str()];

		// Position (資料スライド 12)
		for (uint32_t k = 0; k < nodeAnimAssimp->mNumPositionKeys; ++k) {
			aiVectorKey& key = nodeAnimAssimp->mPositionKeys[k];
			nodeAnim.translate.push_back({ (float)key.mTime / (float)animAssimp->mTicksPerSecond, {-key.mValue.x, key.mValue.y, key.mValue.z} });
		}
		// Rotation (右手->左手変換込み)
		for (uint32_t k = 0; k < nodeAnimAssimp->mNumRotationKeys; ++k) {
			aiQuatKey& key = nodeAnimAssimp->mRotationKeys[k];
			nodeAnim.rotate.push_back({ (float)key.mTime / (float)animAssimp->mTicksPerSecond, {key.mValue.x, -key.mValue.y, -key.mValue.z, key.mValue.w} });
		}
		// Scale
		for (uint32_t k = 0; k < nodeAnimAssimp->mNumScalingKeys; ++k) {
			aiVectorKey& key = nodeAnimAssimp->mScalingKeys[k];
			nodeAnim.scale.push_back({ (float)key.mTime / (float)animAssimp->mTicksPerSecond, {key.mValue.x, key.mValue.y, key.mValue.z} });
		}
	}
	return animation;
}

void Model::LoadModelFile(const std::string& directoryPath, const std::string& filename) {
	loading::Scope timing(loading::Stage::ModelImport);
	Assimp::Importer importer;
	std::string filepath = directoryPath + "/" + filename;
	std::string modelFolder = std::filesystem::path(filepath).parent_path().string();

	const aiScene* scene = importer.ReadFile(filepath.c_str(),
		aiProcess_Triangulate | aiProcess_FlipWindingOrder | aiProcess_FlipUVs);

	if (!scene || !scene->HasMeshes()) {
		std::string errorStr = importer.GetErrorString();
		OutputDebugStringA(("Assimp Error: " + errorStr + "\n").c_str());
		assert(false && "Failed to load model file!");
	}

	// --- 追加：ルートノードからの階層解析 ---
	rootNode_ = ReadNode(scene->mRootNode);

	// 1. マテリアルの解析
	for (uint32_t i = 0; i < scene->mNumMaterials; ++i) {
		aiMaterial* material = scene->mMaterials[i];
		aiString name;
		material->Get(AI_MATKEY_NAME, name);
		ModelMaterialData& matData = modelMaterials_[name.C_Str()];
		if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
			aiString texturePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);
			matData.textureFilePath = modelFolder + "/" + texturePath.C_Str();
		}
	}

	// 2. メッシュの解析
	// 2. メッシュの解析
	for (uint32_t i = 0; i < scene->mNumMeshes; ++i) {
		aiMesh* mesh = scene->mMeshes[i];
		assert(mesh->HasNormals()); // スライド3：法線情報の検証
		assert(mesh->HasTextureCoords(0)); // スライド3：テクスチャ座標の検証

		Mesh currentMesh{};
		currentMesh.start = (uint32_t)indices_.size();
		currentMesh.count = mesh->mNumFaces * 3;

		aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
		aiString matName;
		material->Get(AI_MATKEY_NAME, matName);
		currentMesh.materialName = matName.C_Str();

		// --- 頂点の解析 (スライド3仕様) ---
		// 重複させずに頂点データそのものを格納するため、あらかじめ頂点数分のメモリを確保
		uint32_t vertexBase = (uint32_t)vertices_.size();
		vertices_.resize(vertexBase + mesh->mNumVertices);

		for (uint32_t vertexIndex = 0; vertexIndex < mesh->mNumVertices; ++vertexIndex) {
			VertexData vertex{};
			aiVector3D& position = mesh->mVertices[vertexIndex];
			aiVector3D& normal = mesh->mNormals[vertexIndex];
			aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];

			// 右手系->左手系への変換（x軸反転）
			vertex.position = { -position.x, position.y, position.z, 1.0f };
			vertex.normal = { -normal.x, normal.y, normal.z };
			vertex.texcoord = { texcoord.x, texcoord.y };

			vertices_[vertexBase + vertexIndex] = vertex;
		}

		for (uint32_t boneIndex = 0; boneIndex < mesh->mNumBones; ++boneIndex) {
			const aiBone* bone = mesh->mBones[boneIndex];
			if (!bone) {
				continue;
			}

			const std::string boneName = bone->mName.C_Str();
			JointWeightData& jointWeightData = skinClusterData_[boneName];
			jointWeightData.inverseBindPoseMatrix = ConvertAiMatrix(bone->mOffsetMatrix);

			for (uint32_t weightIndex = 0; weightIndex < bone->mNumWeights; ++weightIndex) {
				const aiVertexWeight& weight = bone->mWeights[weightIndex];
				const uint32_t localVertexIndex = weight.mVertexId;
				if (localVertexIndex >= mesh->mNumVertices) {
					OutputDebugStringA(("Skinning weight has invalid local vertex index. bone=" + boneName + "\n").c_str());
					assert(false && "Assimp bone weight vertex index is out of range.");
					continue;
				}

				const uint32_t globalVertexIndex = vertexBase + localVertexIndex;
				if (globalVertexIndex >= vertices_.size()) {
					OutputDebugStringA(("Skinning weight has invalid global vertex index. bone=" + boneName + "\n").c_str());
					assert(false && "Converted bone weight vertex index is out of range.");
					continue;
				}

				if (weight.mWeight > 0.0f && std::isfinite(weight.mWeight)) {
					jointWeightData.vertexWeights.emplace_back(globalVertexIndex, weight.mWeight);
				}
			}
		}

		// --- インデックスの解析 (スライド4仕様) ---
		// 各面（Face）が参照している頂点インデックス番号を取得して格納
		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {
			aiFace& face = mesh->mFaces[faceIndex];
			assert(face.mNumIndices == 3); // 面が三角形であることの検証

			for (uint32_t element = 0; element < face.mNumIndices; ++element) {
				uint32_t vertexIndex = face.mIndices[element];
				// 複数メッシュに対応するため、全体のオフセット(vertexBase)を加えて登録
				indices_.push_back(vertexIndex + vertexBase);
			}
		}
		meshes_.push_back(currentMesh);
	}
}

/**
 * 再帰的にノードを解析する関数 (資料スライド 5)
 */
void Model::CreateSkinCluster() {
	if (skinClusterData_.empty() || vertices_.empty() || !modelManager_) {
		return;
	}

	DirectXCommon* dxCommon = modelManager_->GetDxCommon();
	SrvManager* srvManager = modelManager_->GetSrvManager();
	assert(dxCommon);
	assert(srvManager);
	assert(vertexResource_);

	const Skeleton bindPoseSkeleton = SkeletonSystem::CreateSkeleton(rootNode_);
	const size_t jointCount = bindPoseSkeleton.joints.size();
	if (jointCount == 0) {
		return;
	}

	for (const auto& [boneName, jointWeightData] : skinClusterData_) {
		(void)jointWeightData;
		if (bindPoseSkeleton.jointMap.find(boneName) == bindPoseSkeleton.jointMap.end()) {
			OutputDebugStringA(("Skinning bone is not found in skeleton: " + boneName + "\n").c_str());
		}
	}

	skinCluster_.jointNames.clear();
	skinCluster_.inverseBindPoseMatrices.clear();
	skinCluster_.jointNames.reserve(jointCount);
	skinCluster_.inverseBindPoseMatrices.reserve(jointCount);

	std::vector<std::vector<std::pair<int32_t, float>>> influences(vertices_.size());
	for (const Joint& joint : bindPoseSkeleton.joints) {
		skinCluster_.jointNames.push_back(joint.name);

		auto it = skinClusterData_.find(joint.name);
		if (it == skinClusterData_.end()) {
			skinCluster_.inverseBindPoseMatrices.push_back(MatrixMath::MakeIdentity4x4());
			continue;
		}

		skinCluster_.inverseBindPoseMatrices.push_back(it->second.inverseBindPoseMatrix);
		for (const auto& [vertexIndex, weight] : it->second.vertexWeights) {
			if (vertexIndex >= influences.size()) {
				OutputDebugStringA(("Skinning vertex weight is out of range. joint=" + joint.name + "\n").c_str());
				assert(false && "Skinning vertex weight index is out of range.");
				continue;
			}
			influences[vertexIndex].emplace_back(joint.index, weight);
		}
	}

	skinCluster_.influenceResource = dxCommon->CreateBufferResource(sizeof(VertexInfluence) * vertices_.size());
	skinCluster_.influenceBufferView.BufferLocation = skinCluster_.influenceResource->GetGPUVirtualAddress();
	skinCluster_.influenceBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexInfluence) * vertices_.size());
	skinCluster_.influenceBufferView.StrideInBytes = sizeof(VertexInfluence);
	HRESULT hr = skinCluster_.influenceResource->Map(0, nullptr, reinterpret_cast<void**>(&skinCluster_.mappedInfluence));
	assert(SUCCEEDED(hr));

	skinCluster_.inputVertexSrvHandle = srvManager->Allocate();
	srvManager->CreateSRVforStructuredBuffer(
		skinCluster_.inputVertexSrvHandle,
		vertexResource_.Get(),
		static_cast<UINT>(vertices_.size()),
		sizeof(VertexData));

	skinCluster_.influenceSrvHandle = srvManager->Allocate();
	srvManager->CreateSRVforStructuredBuffer(
		skinCluster_.influenceSrvHandle,
		skinCluster_.influenceResource.Get(),
		static_cast<UINT>(vertices_.size()),
		sizeof(VertexInfluence));

	for (size_t vertexIndex = 0; vertexIndex < vertices_.size(); ++vertexIndex) {
		VertexInfluence& influence = skinCluster_.mappedInfluence[vertexIndex];
		for (uint32_t i = 0; i < kMaxBoneInfluence; ++i) {
			influence.weights[i] = 0.0f;
			influence.jointIndices[i] = 0;
		}

		auto& vertexInfluences = influences[vertexIndex];
		std::sort(vertexInfluences.begin(), vertexInfluences.end(), [](const auto& lhs, const auto& rhs) {
			return lhs.second > rhs.second;
		});

		const size_t influenceCount = std::min<size_t>(kMaxBoneInfluence, vertexInfluences.size());
		float totalWeight = 0.0f;
		for (size_t influenceIndex = 0; influenceIndex < influenceCount; ++influenceIndex) {
			const int32_t jointIndex = vertexInfluences[influenceIndex].first;
			const float weight = vertexInfluences[influenceIndex].second;
			if (jointIndex < 0 || jointIndex >= static_cast<int32_t>(jointCount)) {
				OutputDebugStringA("Skinning influence has invalid joint index.\n");
				assert(false && "Skinning influence joint index is out of range.");
				continue;
			}

			influence.weights[influenceIndex] = weight;
			influence.jointIndices[influenceIndex] = jointIndex;
			totalWeight += weight;
		}

		if (totalWeight > std::numeric_limits<float>::epsilon() && std::isfinite(totalWeight)) {
			for (uint32_t influenceIndex = 0; influenceIndex < kMaxBoneInfluence; ++influenceIndex) {
				influence.weights[influenceIndex] /= totalWeight;
			}
		} else {
			influence.weights[0] = 1.0f;
			influence.jointIndices[0] = bindPoseSkeleton.root >= 0 ? bindPoseSkeleton.root : 0;
		}
	}

	skinCluster_.paletteResource = dxCommon->CreateBufferResource(sizeof(MatrixPalette) * jointCount);
	hr = skinCluster_.paletteResource->Map(0, nullptr, reinterpret_cast<void**>(&skinCluster_.mappedPalette));
	assert(SUCCEEDED(hr));

	for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
		skinCluster_.mappedPalette[jointIndex].skeletonSpaceMatrix = MatrixMath::MakeIdentity4x4();
		skinCluster_.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix = MatrixMath::MakeIdentity4x4();
	}

	skinCluster_.paletteSrvHandle = srvManager->Allocate();
	srvManager->CreateSRVforStructuredBuffer(
		skinCluster_.paletteSrvHandle,
		skinCluster_.paletteResource.Get(),
		static_cast<UINT>(jointCount),
		sizeof(MatrixPalette));

	skinCluster_.skinnedVertexResource = dxCommon->CreateUAVBufferResource(
		sizeof(VertexData) * vertices_.size(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	skinCluster_.skinnedVertexResourceState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	skinCluster_.skinnedVertexBufferView.BufferLocation = skinCluster_.skinnedVertexResource->GetGPUVirtualAddress();
	skinCluster_.skinnedVertexBufferView.SizeInBytes = static_cast<UINT>(sizeof(VertexData) * vertices_.size());
	skinCluster_.skinnedVertexBufferView.StrideInBytes = sizeof(VertexData);

	skinCluster_.skinnedVertexUavHandle = srvManager->Allocate();
	srvManager->CreateUAVforStructuredBuffer(
		skinCluster_.skinnedVertexUavHandle,
		skinCluster_.skinnedVertexResource.Get(),
		static_cast<UINT>(vertices_.size()),
		sizeof(VertexData));

	skinCluster_.skinningInfoResource = dxCommon->CreateBufferResource(sizeof(SkinningInfo));
	hr = skinCluster_.skinningInfoResource->Map(0, nullptr, reinterpret_cast<void**>(&skinCluster_.mappedSkinningInfo));
	assert(SUCCEEDED(hr));
	skinCluster_.mappedSkinningInfo->vertexCount = static_cast<uint32_t>(vertices_.size());
	skinCluster_.mappedSkinningInfo->jointCount = static_cast<uint32_t>(skinCluster_.jointNames.size());
	skinCluster_.mappedSkinningInfo->padding[0] = 0;
	skinCluster_.mappedSkinningInfo->padding[1] = 0;

	UpdateSkinCluster(bindPoseSkeleton);
}

void Model::UpdateSkinCluster(const Skeleton& skeleton) {
	if (!HasSkinCluster()) {
		return;
	}

	const size_t jointCount = skeleton.joints.size();
	if (jointCount != skinCluster_.jointNames.size() || jointCount != skinCluster_.inverseBindPoseMatrices.size()) {
		OutputDebugStringA("SkinCluster palette size does not match skeleton joint count.\n");
		assert(false && "SkinCluster palette size mismatch.");
		return;
	}

	if (!skinCluster_.mappedPalette) {
		OutputDebugStringA("SkinCluster palette is not mapped.\n");
		assert(false && "SkinCluster palette is not mapped.");
		return;
	}

	for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
		const Joint& joint = skeleton.joints[jointIndex];
		if (skinCluster_.jointNames[jointIndex] != joint.name) {
			OutputDebugStringA(("SkinCluster joint order mismatch: " + skinCluster_.jointNames[jointIndex] + " / " + joint.name + "\n").c_str());
			assert(false && "SkinCluster joint order mismatch.");
			continue;
		}

		Matrix4x4 skinningMatrix = MatrixMath::Multiply(skinCluster_.inverseBindPoseMatrices[jointIndex], joint.skeletonSpaceMatrix);
		if (!IsFiniteMatrix(skinningMatrix)) {
			OutputDebugStringA(("SkinCluster palette matrix has NaN or Inf. joint=" + joint.name + "\n").c_str());
			assert(false && "SkinCluster palette matrix has NaN or Inf.");
			skinningMatrix = MatrixMath::MakeIdentity4x4();
		}

		skinCluster_.mappedPalette[jointIndex].skeletonSpaceMatrix = skinningMatrix;
		skinCluster_.mappedPalette[jointIndex].skeletonSpaceInverseTransposeMatrix = MatrixMath::Transpose(MatrixMath::Inverse(skinningMatrix));
	}
}

bool Model::HasComputeSkinningResources() const {
	return
		HasSkinCluster() &&
		skinCluster_.inputVertexSrvHandle != SkinCluster::kInvalidSrvHandle &&
		skinCluster_.influenceSrvHandle != SkinCluster::kInvalidSrvHandle &&
		skinCluster_.paletteSrvHandle != SkinCluster::kInvalidSrvHandle &&
		skinCluster_.skinnedVertexUavHandle != SkinCluster::kInvalidSrvHandle &&
		skinCluster_.skinnedVertexResource.Get() != nullptr &&
		skinCluster_.skinningInfoResource.Get() != nullptr;
}

bool Model::UseComputeSkinning() const {
	return skinCluster_.useComputeSkinning && HasComputeSkinningResources();
}

void Model::DispatchComputeSkinning(Object3dCommon* object3dCommon) {
	if (!UseComputeSkinning()) {
		return;
	}
	assert(object3dCommon);
	assert(!vertices_.empty());

	ID3D12GraphicsCommandList* commandList = object3dCommon->GetDxCommon()->GetCommandList();
	SrvManager* srvManager = object3dCommon->GetSrvManager();
	assert(commandList);
	assert(srvManager);
	assert(skinCluster_.mappedSkinningInfo);

	skinCluster_.mappedSkinningInfo->vertexCount = static_cast<uint32_t>(vertices_.size());
	skinCluster_.mappedSkinningInfo->jointCount = static_cast<uint32_t>(skinCluster_.jointNames.size());

	TransitionResource(
		commandList,
		skinCluster_.skinnedVertexResource.Get(),
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	skinCluster_.skinnedVertexResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

	commandList->SetComputeRootSignature(object3dCommon->GetSkinningComputeRootSignature());
	commandList->SetPipelineState(object3dCommon->GetSkinningComputePipelineState());
	commandList->SetComputeRootConstantBufferView(0, skinCluster_.skinningInfoResource->GetGPUVirtualAddress());
	commandList->SetComputeRootDescriptorTable(1, srvManager->GetGPUDescriptorHandle(skinCluster_.inputVertexSrvHandle));
	commandList->SetComputeRootDescriptorTable(2, srvManager->GetGPUDescriptorHandle(skinCluster_.influenceSrvHandle));
	commandList->SetComputeRootDescriptorTable(3, srvManager->GetGPUDescriptorHandle(skinCluster_.paletteSrvHandle));
	commandList->SetComputeRootDescriptorTable(4, srvManager->GetGPUDescriptorHandle(skinCluster_.skinnedVertexUavHandle));

	const uint32_t threadCount = 64;
	const uint32_t dispatchCount = (static_cast<uint32_t>(vertices_.size()) + threadCount - 1) / threadCount;
	commandList->Dispatch(dispatchCount, 1, 1);

	TransitionResource(
		commandList,
		skinCluster_.skinnedVertexResource.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	skinCluster_.skinnedVertexResourceState = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
}

Model::Node Model::ReadNode(aiNode* node) {
	Node result;

	// 名前を取得
	result.name = node->mName.C_Str();

	// Assimpの行列からSRTを抽出
	aiVector3D scale, translate;
	aiQuaternion rotate;
	node->mTransformation.Decompose(scale, rotate, translate);

	// 右手系から左手系への変換を適用して代入
	result.transform.scale = { scale.x, scale.y, scale.z };
	result.transform.rotate = { rotate.x, -rotate.y, -rotate.z, rotate.w };
	result.transform.translate = { -translate.x, translate.y, translate.z };

	// 抽出したトランスフォームから localMatrix を再構築
	result.localMatrix = MatrixMath::Multiply(MatrixMath::MakeScaleMatrix(result.transform.scale),
		MatrixMath::Multiply(MatrixMath::MakeRotateMatrix(result.transform.rotate),
			MatrixMath::MakeTranslateMatrix(result.transform.translate)));

	// 子供の数だけメモリを確保
	result.children.resize(node->mNumChildren);

	// 再帰処理
	for (uint32_t i = 0; i < node->mNumChildren; ++i) {
		result.children[i] = ReadNode(node->mChildren[i]);
	}

	return result;
}

/**
 * テクスチャの一括読み込み
 */
void Model::LoadTextures() {
	for (auto& pair : modelMaterials_) {
		if (!pair.second.textureFilePath.empty()) {
			pair.second.textureHandle = TextureManager::GetInstance()->LoadTexture2D(pair.second.textureFilePath);
		}
	}
}

/**
 * 描画
 */
void Model::Draw(DirectXCommon* dxCommon) {
	DrawGeometry(dxCommon, true);
}

void Model::DrawDepth(DirectXCommon* dxCommon) {
	DrawGeometry(dxCommon, false);
}

void Model::DrawGeometry(DirectXCommon* dxCommon, bool bindMaterialTextures) {
	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();
	if (UseComputeSkinning()) {
		commandList->IASetVertexBuffers(0, 1, &skinCluster_.skinnedVertexBufferView);
	} else if (HasSkinCluster()) {
		D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = { vertexBufferView_, skinCluster_.influenceBufferView };
		commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
	} else {
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	}
	commandList->IASetIndexBuffer(&indexBufferView_);

	if (meshes_.empty()) {
		commandList->DrawIndexedInstanced(indexCount_, 1, 0, 0, 0);
	}
	else {
		for (const auto& mesh : meshes_) {
			if (bindMaterialTextures) {
				auto it = modelMaterials_.find(mesh.materialName);
				if (it != modelMaterials_.end() && !it->second.textureFilePath.empty()) {
					D3D12_GPU_DESCRIPTOR_HANDLE handle = TextureManager::GetInstance()->GetGpuHandle(it->second.textureHandle);
					commandList->SetGraphicsRootDescriptorTable(5, handle);
				}
			}
			commandList->DrawIndexedInstanced(mesh.count, 1, mesh.start, 0, 0);
		}
	}
}

/**
 * ★追加：インスタンシング描画
 */
void Model::Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount) {
	// 頂点バッファとインデックスバッファをセット
	if (UseComputeSkinning()) {
		commandList->IASetVertexBuffers(0, 1, &skinCluster_.skinnedVertexBufferView);
	} else if (HasSkinCluster()) {
		D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = { vertexBufferView_, skinCluster_.influenceBufferView };
		commandList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
	} else {
		commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	}
	commandList->IASetIndexBuffer(&indexBufferView_);

	if (meshes_.empty()) {
		// メッシュ情報がない場合は全体をそのまま描画
		commandList->DrawIndexedInstanced(indexCount_, instanceCount, 0, 0, 0);
	}
	else {
		for (const auto& mesh : meshes_) {
			// パーティクルシステム側でテクスチャを管理しているため、
			// ここではメッシュごとのテクスチャ切り替えは行わず、指定された数だけ描画します。
			commandList->DrawIndexedInstanced(mesh.count, instanceCount, mesh.start, 0, 0);
		}
	}
}
