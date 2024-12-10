#pragma once
#include <vector>
#include <string>
#include "Common.h"
#include "Mesh.h"

struct aiNode;
struct aiMesh;
struct aiScene;

class Model
{
	std::vector<Mesh> m_meshes;
	ID3D11ShaderResourceView* m_pTextureAlbedo = nullptr;
	ID3D11ShaderResourceView* m_pTextureMetalness = nullptr;
	ID3D11ShaderResourceView* m_pTextureRoughness = nullptr;
	ID3D11ShaderResourceView* m_pTextureNormal = nullptr;

	void processNode(aiNode* node, const aiScene* scene, ID3D11Device* d3dDevice);
	Mesh processMesh(aiMesh* mesh, const aiScene* scene, ID3D11Device* d3dDevice);
public:
	Model(ID3D11Device* d3dDevice, const std::string& filePath, const std::wstring& albedoTexPath, const std::wstring& normalTexPath, const std::wstring& roughnessTexPath, const std::wstring& metallicTexPath);
	Model(const Model& other) = delete;
	~Model();

	void Draw(ID3D11DeviceContext* d3dContext, ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader);

	enum TextureType
	{
		ALBEDO,
		NORMAL,
		ROUGHNESS,
		METALLIC
	};

	void changeTexture(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dContext, const std::string& newTexPath, TextureType type);
};

