#pragma once
#include <vector>
#include <string>
#include "Common.h"

class Mesh
{
	std::vector<Vertex> m_vertices;
	std::vector<unsigned int> m_indices;

	ID3D11Device* m_d3dDevice = nullptr;

	ID3D11Buffer* m_pVertexBuffer = nullptr;
	ID3D11Buffer* m_pIndexBuffer = nullptr;

	int Init(ID3D11Device* d3dDevice);
public:
	Mesh(ID3D11Device* d3dDevice, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);

	Mesh(const Mesh& other);
	Mesh(Mesh&& other) noexcept;

	~Mesh();

	void Draw(ID3D11DeviceContext* d3dContext, ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader);

	size_t GetNumVertices() { return m_vertices.size(); }
	Vertex* GetVertices() { return m_vertices.data(); }

	size_t GetNumIndices() { return m_indices.size(); }
	unsigned int* GetIndices() { return m_indices.data(); }
};

