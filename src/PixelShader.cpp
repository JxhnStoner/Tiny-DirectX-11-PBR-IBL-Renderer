#include "PixelShader.h"
#include "Common.h"

PixelShader::PixelShader(ID3D11Device* d3dDevice, const std::string path, const std::string entryPoint)
{
	ID3DBlob* pPSBlob = NULL;
	
	std::wstring wPath{ path.begin(), path.end() };

	HRESULT hr;
	hr = CompileShaderFromFile(wPath.c_str(), entryPoint.c_str(), "ps_5_0", &pPSBlob);
	if (FAILED(hr))
	{
		if (pPSBlob) pPSBlob->Release();
		std::cout << "Can't compile shader file " << path << "\n";
		return;
	}

	hr = d3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &m_pPixelShader);
	if (FAILED(hr)) return;
}

void PixelShader::Bind(ID3D11DeviceContext* deviceContext)
{
	deviceContext->PSSetShader(m_pPixelShader, NULL, 0);
}

void PixelShader::Unbind(ID3D11DeviceContext* deviceContext)
{
	deviceContext->PSSetShader(nullptr, NULL, 0);
}
