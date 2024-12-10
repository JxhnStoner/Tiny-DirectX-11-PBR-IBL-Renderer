#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include <iostream>

using namespace DirectX;

struct Vertex
{
	XMFLOAT4 Pos;
	XMFLOAT2 Tex;
	XMFLOAT3 Normal;
};

HRESULT CompileShaderFromFile(const wchar_t* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut);