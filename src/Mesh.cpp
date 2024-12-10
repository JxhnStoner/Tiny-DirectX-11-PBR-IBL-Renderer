#include "Mesh.h"

Mesh::Mesh(ID3D11Device* d3dDevice, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
	: m_vertices(vertices), m_indices(indices)
{
	m_d3dDevice = d3dDevice;
	Init(d3dDevice);
}

Mesh::Mesh(const Mesh& other)
{
	m_vertices = other.m_vertices;
	m_indices = other.m_indices;
	m_d3dDevice = other.m_d3dDevice;
	Init(m_d3dDevice);
}

Mesh::Mesh(Mesh&& other) noexcept
{
	m_vertices = other.m_vertices;
	m_indices = other.m_indices;
	m_d3dDevice = other.m_d3dDevice;
	m_pVertexBuffer = other.m_pVertexBuffer;
	m_pIndexBuffer = other.m_pIndexBuffer;
	other.m_pVertexBuffer = nullptr;
	other.m_pIndexBuffer = nullptr;	
}

Mesh::~Mesh()
{
	if (m_pVertexBuffer) m_pVertexBuffer->Release();
	if (m_pIndexBuffer) m_pIndexBuffer->Release();
}

int Mesh::Init(ID3D11Device* d3dDevice)
{
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex) * m_vertices.size();
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = m_vertices.data();

	HRESULT hr;
	hr = d3dDevice->CreateBuffer(&bd, &InitData, &m_pVertexBuffer);
	if (FAILED(hr)) return hr;

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(UINT) * m_indices.size();
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;

	InitData.pSysMem = m_indices.data();

	hr = d3dDevice->CreateBuffer(&bd, &InitData, &m_pIndexBuffer);
	if (FAILED(hr)) return hr;
}

void Mesh::Draw(ID3D11DeviceContext* d3dContext, ID3D11VertexShader* vertexShader, ID3D11PixelShader* pixelShader)
{
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	d3dContext->IASetVertexBuffers(0, 1, &m_pVertexBuffer, &stride, &offset);
	d3dContext->IASetIndexBuffer(m_pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	d3dContext->VSSetShader(vertexShader, nullptr, 0);
	d3dContext->PSSetShader(pixelShader, nullptr, 0);
	d3dContext->DrawIndexed(m_indices.size(), 0, 0);
}




