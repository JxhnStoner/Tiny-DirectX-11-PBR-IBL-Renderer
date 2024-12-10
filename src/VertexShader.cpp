#include "VertexShader.h"
#include "Common.h"


VertexShader::VertexShader(ID3D11Device* d3dDevice, const std::string path, const std::string entryPoint)
{
	HRESULT hr = S_OK;

	ID3DBlob* pVSBlob = NULL;

	std::wstring wPath{ path.begin(), path.end() };

	hr = CompileShaderFromFile(wPath.c_str(), entryPoint.c_str(), "vs_5_0", &pVSBlob);
	if (FAILED(hr))
	{
		if (pVSBlob) pVSBlob->Release();
		std::cout << "Can't compile shader file " << path << "!\n";
		return;
	}

	hr = d3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &m_pVertexShader);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return;
	}
}

void VertexShader::Bind(ID3D11DeviceContext* deviceContext)
{
	deviceContext->VSSetShader(m_pVertexShader, NULL, 0);
}

void VertexShader::Unbind(ID3D11DeviceContext* deviceContext)
{
	deviceContext->VSSetShader(nullptr, NULL, 0);
}
