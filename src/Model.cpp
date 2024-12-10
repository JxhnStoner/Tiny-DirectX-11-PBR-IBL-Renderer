#include "Model.h"
#include <iostream>
#include <string>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <WICTextureLoader.h>
#include <DirectXTex.h>

Model::Model(ID3D11Device* d3dDevice, const std::string& filePath, const std::wstring& albedoTexPath, const std::wstring& normalTexPath, const std::wstring& roughnessTexPath, const std::wstring& metallicTexPath)
{
	Assimp::Importer importer;
	const aiScene* scene = importer.ReadFile(filePath, aiProcess_Triangulate | aiProcess_FlipUVs);
	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		std::cout << "Can't load model!\n";
		return;
	}
	processNode(scene->mRootNode, scene, d3dDevice);

	if (scene->HasMaterials())
	{
		for (int i = 0; i < scene->mNumMaterials; i++)
		{
			aiMaterial* material = scene->mMaterials[i];
			std::cout << "Material Name: " << '\"' << material->GetName().C_Str() << "\" {" << std::endl;

			aiTextureType textureTypes[]{

				/** The texture is combined with the result of the diffuse
				*  lighting equation.
				*  OR
				*  PBR Specular/Glossiness
				*/
				aiTextureType_DIFFUSE,

				/** The texture is combined with the result of the specular
				 *  lighting equation.
				 *  OR
				 *  PBR Specular/Glossiness
				 */
				aiTextureType_SPECULAR,

				/** The texture is combined with the result of the ambient
				 *  lighting equation.
				 */
				aiTextureType_AMBIENT,

				/** The texture is added to the result of the lighting
				 *  calculation. It isn't influenced by incoming light.
				 */
				aiTextureType_EMISSIVE,

				/** The texture is a height map.
				 *
				 *  By convention, higher gray-scale values stand for
				 *  higher elevations from the base height.
				 */
				aiTextureType_HEIGHT,

				/** The texture is a (tangent space) normal-map.
				 *
				 *  Again, there are several conventions for tangent-space
				 *  normal maps. Assimp does (intentionally) not
				 *  distinguish here.
				 */
				aiTextureType_NORMALS,

				/** The texture defines the glossiness of the material.
				 *
				 *  The glossiness is in fact the exponent of the specular
				 *  (phong) lighting equation. Usually there is a conversion
				 *  function defined to map the linear color values in the
				 *  texture to a suitable exponent. Have fun.
				*/
				aiTextureType_SHININESS,

				/** The texture defines per-pixel opacity.
				 *
				 *  Usually 'white' means opaque and 'black' means
				 *  'transparency'. Or quite the opposite. Have fun.
				*/
				aiTextureType_OPACITY,

				/** Displacement texture
				 *
				 *  The exact purpose and format is application-dependent.
				 *  Higher color values stand for higher vertex displacements.
				*/
				aiTextureType_DISPLACEMENT,

				/** Lightmap texture (aka Ambient Occlusion)
				 *
				 *  Both 'Lightmaps' and dedicated 'ambient occlusion maps' are
				 *  covered by this material property. The texture contains a
				 *  scaling value for the final color value of a pixel. Its
				 *  intensity is not affected by incoming light.
				*/
				aiTextureType_LIGHTMAP,

				/** Reflection texture
				 *
				 * Contains the color of a perfect mirror reflection.
				 * Rarely used, almost never for real-time applications.
				*/
				aiTextureType_REFLECTION,

				/** PBR Materials
				 * PBR definitions from maya and other modelling packages now use this standard.
				 * This was originally introduced around 2012.
				 * Support for this is in game engines like Godot, Unreal or Unity3D.
				 * Modelling packages which use this are very common now.
				 */

				aiTextureType_BASE_COLOR,
				aiTextureType_NORMAL_CAMERA,
				aiTextureType_EMISSION_COLOR,
				aiTextureType_METALNESS,
				aiTextureType_DIFFUSE_ROUGHNESS,
				aiTextureType_AMBIENT_OCCLUSION,

				/** PBR Material Modifiers
				* Some modern renderers have further PBR modifiers that may be overlaid
				* on top of the 'base' PBR materials for additional realism.
				* These use multiple texture maps, so only the base type is directly defined
				*/

				/** Sheen
				* Generally used to simulate textiles that are covered in a layer of microfibers
				* eg velvet
				* https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_materials_sheen
				*/
				aiTextureType_SHEEN,

				/** Clearcoat
				* Simulates a layer of 'polish' or 'lacquer' layered on top of a PBR substrate
				* https://autodesk.github.io/standard-surface/#closures/coating
				* https://github.com/KhronosGroup/glTF/tree/master/extensions/2.0/Khronos/KHR_materials_clearcoat
				*/
				aiTextureType_CLEARCOAT,

				/** Transmission
				* Simulates transmission through the surface
				* May include further information such as wall thickness
				*/
				aiTextureType_TRANSMISSION
			};

			for (int j = 0; j < ARRAYSIZE(textureTypes); j++)
			{
				int numTextures = material->GetTextureCount(textureTypes[j]);
				if (numTextures > 0)
				{
					aiString path;
					material->GetTexture(textureTypes[j], 0, &path);
					std::cout << '\t' << "Map Type: \"" << textureTypes[j] << "\"" << ", Map Path: " << '\"' << path.C_Str() << '\"' << std::endl;
				}
			}
			std::cout << "};" << std::endl;
		}
	}

	HRESULT hr;
	if (albedoTexPath.find(L".tga") != std::string::npos || albedoTexPath.find(L".TGA") != std::string::npos)
	{
		auto image = std::make_unique<ScratchImage>();
		hr = LoadFromTGAFile(albedoTexPath.c_str(), TGA_FLAGS_NONE, nullptr, *image);
		if (FAILED(hr)) return;
		hr = CreateShaderResourceView(d3dDevice, (*image).GetImages(), (*image).GetImageCount(),
			(*image).GetMetadata(), &m_pTextureAlbedo);
		if (FAILED(hr)) return;

		hr = LoadFromTGAFile(normalTexPath.c_str(), TGA_FLAGS_NONE, nullptr, *image);
		if (FAILED(hr)) return;
		hr = CreateShaderResourceView(d3dDevice, (*image).GetImages(), (*image).GetImageCount(),
			(*image).GetMetadata(), &m_pTextureNormal);
		if (FAILED(hr)) return;

		hr = LoadFromTGAFile(roughnessTexPath.c_str(), TGA_FLAGS_NONE, nullptr, *image);
		if (FAILED(hr)) return;
		hr = CreateShaderResourceView(d3dDevice, (*image).GetImages(), (*image).GetImageCount(),
			(*image).GetMetadata(), &m_pTextureRoughness);
		if (FAILED(hr)) return;

		hr = LoadFromTGAFile(metallicTexPath.c_str(), TGA_FLAGS_NONE, nullptr, *image);
		if (FAILED(hr)) return;
		hr = CreateShaderResourceView(d3dDevice, (*image).GetImages(), (*image).GetImageCount(),
			(*image).GetMetadata(), &m_pTextureMetalness);
		if (FAILED(hr)) return;
	}
	else
	{
		hr = CreateWICTextureFromFile(d3dDevice, albedoTexPath.c_str(), nullptr, &m_pTextureAlbedo);
		if (FAILED(hr)) return;

		hr = CreateWICTextureFromFile(d3dDevice, normalTexPath.c_str(), nullptr, &m_pTextureNormal);
		if (FAILED(hr)) return;

		hr = CreateWICTextureFromFile(d3dDevice, roughnessTexPath.c_str(), nullptr, &m_pTextureRoughness);
		if (FAILED(hr)) return;

		hr = CreateWICTextureFromFile(d3dDevice, metallicTexPath.c_str(), nullptr, &m_pTextureMetalness);
		if (FAILED(hr)) return;
	}
}

Model::~Model()
{
	if (m_pTextureAlbedo) m_pTextureAlbedo->Release();
	if (m_pTextureMetalness) m_pTextureMetalness->Release();
	if (m_pTextureRoughness) m_pTextureRoughness->Release();
	if (m_pTextureNormal) m_pTextureNormal->Release();
}

void Model::Draw(ID3D11DeviceContext* d3dContext, ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader)
{
	d3dContext->PSSetShaderResources(0, 1, &m_pTextureAlbedo);
	d3dContext->PSSetShaderResources(1, 1, &m_pTextureNormal);
	d3dContext->PSSetShaderResources(2, 1, &m_pTextureRoughness);
	d3dContext->PSSetShaderResources(3, 1, &m_pTextureMetalness);

	for (auto& mesh : m_meshes)
		mesh.Draw(d3dContext, vertexShader, pixelShader);;
}

void Model::changeTexture(ID3D11Device* d3dDevice, ID3D11DeviceContext* d3dContext, const std::string& newTexPath, TextureType type)
{
	std::wstring path(newTexPath.begin(), newTexPath.end());
	HRESULT hr;
	switch (type)
	{
	case ALBEDO:
		hr = CreateWICTextureFromFile(d3dDevice, path.c_str(), nullptr, &m_pTextureAlbedo);
		if (FAILED(hr)) return;
		break;
	case NORMAL:
		hr = CreateWICTextureFromFile(d3dDevice, path.c_str(), nullptr, &m_pTextureNormal);
		if (FAILED(hr)) return;
		break;
	case ROUGHNESS:
		hr = CreateWICTextureFromFile(d3dDevice, path.c_str(), nullptr, &m_pTextureRoughness);
		if (FAILED(hr)) return;
		break;
	case METALLIC:
		hr = CreateWICTextureFromFile(d3dDevice, path.c_str(), nullptr, &m_pTextureMetalness);
		if (FAILED(hr)) return;
		break;
	}
}

void Model::processNode(aiNode* node, const aiScene* scene, ID3D11Device* d3dDevice)
{
	// process all the node's meshes (if any)
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
		std::cout << "Mesh Material Index: " << mesh->mMaterialIndex << std::endl;
		m_meshes.push_back(processMesh(mesh, scene, d3dDevice));
	}
	// then do the same for each of its children
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		processNode(node->mChildren[i], scene, d3dDevice);
	}
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* scene, ID3D11Device* d3dDevice)
{
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;

	vertices.reserve(mesh->mNumVertices);

	for (unsigned int i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex;
		vertex.Pos.x = mesh->mVertices[i].x;
		vertex.Pos.y = mesh->mVertices[i].y;
		vertex.Pos.z = mesh->mVertices[i].z;

		if (mesh->mTextureCoords[0])
		{
			vertex.Tex.x = mesh->mTextureCoords[0][i].x;
			vertex.Tex.y = mesh->mTextureCoords[0][i].y;
		}
		else
			vertex.Tex = XMFLOAT2(0.f, 0.f);
		vertex.Normal.x = mesh->mNormals[i].x;
		vertex.Normal.y = mesh->mNormals[i].y;
		vertex.Normal.z = mesh->mNormals[i].z;

		vertices.push_back(std::move(vertex));
	}
	// process indices
	for (unsigned int i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned int j = 0; j < face.mNumIndices; j++)
			indices.push_back(face.mIndices[j]);
	}
 
	return Mesh(d3dDevice, vertices, indices);
}