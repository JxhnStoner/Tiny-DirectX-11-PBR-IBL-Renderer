#pragma once
#include <d3d11.h>


class RenderTexture
{
public:
	RenderTexture();
	~RenderTexture();

	bool Initialise(ID3D11Device* device, int width, int height, int mipMaps, DXGI_FORMAT format);

	void SetRenderTarget(ID3D11DeviceContext* deviceContext, ID3D11DepthStencilView* depthStencilView) const;
	void ClearRenderTarget(ID3D11DeviceContext* deviceContext, ID3D11DepthStencilView* depthStencilView, float red,
		float green, float blue, float alpha) const;
	ID3D11ShaderResourceView* GetSRV() const;
	ID3D11Texture2D* GetTexture() const;

private:
	int _width;
	int _height;
	ID3D11RenderTargetView* _pRenderTargetView = nullptr;
	ID3D11Texture2D* _pRenderTargetTexture = nullptr;
	ID3D11ShaderResourceView* _pShaderResourceView = nullptr;
};
