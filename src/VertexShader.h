#pragma once
#include <string>

class ID3D11VertexShader;
class ID3D11Device;
class ID3D11DeviceContext;


class VertexShader
{
	ID3D11VertexShader* m_pVertexShader = nullptr;

public:
	VertexShader(ID3D11Device* d3dDevice, const std::string path, const std::string entryPoint);

	void Bind(ID3D11DeviceContext* deviceContext);
	void Unbind(ID3D11DeviceContext* deviceContext);
};

