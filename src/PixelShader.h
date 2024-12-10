#pragma once
#include <string>

class ID3D11PixelShader;
class ID3D11Device;
class ID3D11DeviceContext;

class PixelShader
{
	ID3D11PixelShader* m_pPixelShader = nullptr;

public:
	PixelShader(ID3D11Device* d3dDevice, const std::string path, const std::string entryPoint);

	void Bind(ID3D11DeviceContext* deviceContext);
	void Unbind(ID3D11DeviceContext* deviceContext);
};

