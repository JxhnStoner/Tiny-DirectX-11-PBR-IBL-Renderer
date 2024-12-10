#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "runtimeobject.lib")

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <wrl\wrappers\corewrappers.h>
#include <WICTextureLoader.h>
#include <shobjidl.h> 

#include "Common.h"
#include "Model.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_dx11.h"
#include "imgui_stdlib.h"

#include <windowsx.h>
#include <iostream>

#include "GLFW/glfw3.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_NATIVE_INCLUDE_NONE
#include "GLFW/glfw3native.h"

#include "ImGuizmo.h"
#include "stb_image.h"
#include <DirectXTex.h>
#include <ScreenGrab.h>

#include "Camera.h"
#include "RenderTexure.h"

#define SCR_WIDTH 1280
#define SCR_HEIGHT 720

GLFWwindow* g_pWindow = nullptr;
HINSTANCE g_hInst = NULL;
HWND g_hWnd = NULL;
D3D_DRIVER_TYPE g_driverType;
D3D_FEATURE_LEVEL g_featureLevel = D3D_FEATURE_LEVEL_11_0;

ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pDeviceContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_pRenderTargetView = nullptr;
ID3D11Texture2D* g_pDepthStencil = nullptr;
ID3D11DepthStencilView* g_pDepthStencilView = nullptr;
ID3D11RasterizerState* g_pRasterState = nullptr;

UINT g_SampleCount = 1;

ID3D11VertexShader* g_pVertexShader = nullptr;
ID3D11PixelShader* g_pPixelShader = nullptr;
ID3D11PixelShader* g_pPixelShaderSolid = nullptr;
ID3D11InputLayout* g_pInputLayout = nullptr;
ID3D11Buffer* g_pVertexBuffer = nullptr;
ID3D11Buffer* g_pIndexBuffer = nullptr;
ID3D11Buffer* g_pCBMatrixes = nullptr;
ID3D11Buffer* g_pCBLight = nullptr;

FLOAT t = 0.0f;

XMFLOAT4 vLightDirs[2];
XMFLOAT4 vLightColors[2];

ID3D11ShaderResourceView* g_pTextureAlbedo = nullptr;
ID3D11ShaderResourceView* g_pTextureMetalness = nullptr;
ID3D11ShaderResourceView* g_pTextureRoughness = nullptr;
ID3D11ShaderResourceView* g_pTextureNormal = nullptr;

ID3D11SamplerState* g_pSamplerLinear = nullptr;

using namespace DirectX;

XMMATRIX g_World;
XMMATRIX g_View;
XMMATRIX g_Projection;

#define MX_SETWORLD 0x101

struct ConstantBufferMatrixes
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
	XMMATRIX mTransInvWorld;
};

struct ConstantBufferLight
{
	XMFLOAT4 vLightDir[2];
	XMFLOAT4 vLightColor[2];
	XMFLOAT4 vCamPos;
};

HRESULT InitDeivce();
HRESULT InitGeometry();
HRESULT InitMatrixes();

void UpdateLight();
void UpdateMatrix(UINT nLightIndex);
void Render();
void CleanupDevice();

void ImguiInit();
void ImguiFrame();
void ImguiRender();
void ImguiCleanup();

float matrixTranslation[3], matrixRotation[3], matrixScale[3];
float g_modelScale = 1.f;
bool g_ImguizmoEnabled = true;

XMMATRIX g_modelMatrix = XMMatrixScaling(0.01f, 0.01f, 0.01f);

unsigned char modelBuffer[sizeof(Model)];
Model* model;

// camera
Camera camera(XMFLOAT3(0.0f, 0.0f, 3.0f));
float lastX = SCR_WIDTH / 2.0;
float lastY = SCR_HEIGHT / 2.0;

bool viewPortActive = false;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

std::string sSelectedFile;
std::string sFilePath;
bool openFile(std::string& pathRef)
{
	//  CREATE FILE OBJECT INSTANCE
	HRESULT f_SysHr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(f_SysHr))
		return FALSE;

	// CREATE FileOpenDialog OBJECT
	IFileOpenDialog* f_FileSystem;
	f_SysHr = CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&f_FileSystem));
	if (FAILED(f_SysHr)) {
		CoUninitialize();
		return FALSE;
	}

	//  SHOW OPEN FILE DIALOG WINDOW
	f_SysHr = f_FileSystem->Show(NULL);
	if (FAILED(f_SysHr)) {
		f_FileSystem->Release();
		CoUninitialize();
		return FALSE;
	}

	//  RETRIEVE FILE NAME FROM THE SELECTED ITEM
	IShellItem* f_Files;
	f_SysHr = f_FileSystem->GetResult(&f_Files);
	if (FAILED(f_SysHr)) {
		f_FileSystem->Release();
		CoUninitialize();
		return FALSE;
	}

	//  STORE AND CONVERT THE FILE NAME
	PWSTR f_Path;
	f_SysHr = f_Files->GetDisplayName(SIGDN_FILESYSPATH, &f_Path);
	if (FAILED(f_SysHr)) {
		f_Files->Release();
		f_FileSystem->Release();
		CoUninitialize();
		return FALSE;
	}

	//  FORMAT AND STORE THE FILE PATH
	std::wstring path(f_Path);
	std::string c(path.begin(), path.end());
	sFilePath = c;

	pathRef = c;

	//  FORMAT STRING FOR EXECUTABLE NAME
	const size_t slash = sFilePath.find_last_of("/\\");
	sSelectedFile = sFilePath.substr(slash + 1);

	//  SUCCESS, CLEAN UP
	CoTaskMemFree(f_Path);
	f_Files->Release();
	f_FileSystem->Release();
	CoUninitialize();
	return TRUE;
}

std::string modelPath, albedoPath, normalPath, roughnessPath, metallicPath;


// hdr to cubemap
ID3D11ShaderResourceView* g_pHDRShaderResourceView = nullptr;
ID3D11VertexShader* g_pRectToCubeVertexShader = nullptr;
ID3D11PixelShader* g_pRectToCubePixelShader = nullptr;

ID3D11DepthStencilState* g_pHDRdepthStencilState = nullptr;
ID3D11RasterizerState* g_pHDRrasterizerState = nullptr;

struct ConstantBufferHDRMatrixes
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
};
ID3D11Buffer* g_pCubemapCBMatrixes = nullptr;

ID3D11ShaderResourceView* g_pCubemapShaderResourceView = nullptr;
ID3D11VertexShader* g_pCubemapVertexShader = nullptr;
ID3D11PixelShader* g_pCubemapPixelShader = nullptr;

ID3D11ShaderResourceView* g_pIrradianceShaderResourceView = nullptr;
ID3D11VertexShader* g_pIrradianceVertexShader = nullptr;
ID3D11PixelShader* g_pIrradiancePixelShader = nullptr;

struct ConstantBufferPreFilterMatrixes
{
	XMMATRIX mWorld;
	XMMATRIX mView;
	XMMATRIX mProjection;
	XMFLOAT4 mCustomData;
};
ID3D11Buffer* g_pPreFilterCBMatrixes = nullptr;

ID3D11ShaderResourceView* g_pPreFilterShaderResourceView = nullptr;
ID3D11VertexShader* g_pPreFilterVertexShader = nullptr;
ID3D11PixelShader* g_pPreFilterPixelShader = nullptr;

ID3D11Texture2D* g_pBRDFTexture = nullptr;
ID3D11ShaderResourceView* g_pBRDFShaderResourceView = nullptr;
ID3D11VertexShader* g_pBRDFVertexShader = nullptr;
ID3D11PixelShader* g_pBRDFPixelShader = nullptr;

void load_shader(const std::string& vsFileName, const std::string& psFileName, ID3D11VertexShader*& vertexShader, ID3D11PixelShader*& pixelShader)
{
	HRESULT hr;
	std::wstring vsFileNameW{ vsFileName.begin(), vsFileName.end() };
	std::wstring psFileNameW{ psFileName.begin(), psFileName.end() };

	ID3DBlob* pVSBlob = NULL;

	hr = CompileShaderFromFile(vsFileNameW.c_str(), "VSMain", "vs_5_0", &pVSBlob);
	if (FAILED(hr))
	{
		if (pVSBlob) pVSBlob->Release();
		std::wstring errorMessage = L"Can't compile VS file " + vsFileNameW;
		MessageBox(g_hWnd, errorMessage.c_str(), TEXT("Error"), MB_OK);
		return;
	}

	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &vertexShader);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return;
	}

	ID3DBlob* pPSBlob = NULL;

	hr = CompileShaderFromFile(psFileNameW.c_str(), "PSMain", "ps_5_0", &pPSBlob);
	if (FAILED(hr))
	{
		if (pPSBlob) pPSBlob->Release();
		std::wstring errorMessage = L"Can't compile PS file " + psFileNameW;
		MessageBox(g_hWnd, errorMessage.c_str(), TEXT("Error"), MB_OK);
		return;
	}

	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &pixelShader);
	if (FAILED(hr))
	{
		pPSBlob->Release();
		return;
	}

	if (pVSBlob) pVSBlob->Release();
	if (pPSBlob) pPSBlob->Release();
}
void renderCube()
{
	ID3D11Buffer* pCubeVertexBuffer, * pCubeIndexBuffer;
	// Создание буфера вершин (по 4 точки на каждую сторону куба, всего 24 вершины)
	Vertex vertices[] =
	{    /* координаты X, Y, Z            координаты текстры tu, tv   нормаль X, Y, Z        */
		{ XMFLOAT4(-1.0f, 1.0f, -1.0f, 1.0f),      XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},
		{ XMFLOAT4(1.0f, 1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},
		{ XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},
		{ XMFLOAT4(-1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},

		{ XMFLOAT4(-1.0f, -1.0f, -1.0f, 1.0f),     XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},
		{ XMFLOAT4(1.0f, -1.0f, -1.0f, 1.0f),      XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},
		{ XMFLOAT4(1.0f, -1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},
		{ XMFLOAT4(-1.0f, -1.0f, 1.0f, 1.0f),      XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},

		{ XMFLOAT4(-1.0f, -1.0f, 1.0f, 1.0f),      XMFLOAT2(0.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},
		{ XMFLOAT4(-1.0f, -1.0f, -1.0f, 1.0f),     XMFLOAT2(1.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},
		{ XMFLOAT4(-1.0f, 1.0f, -1.0f, 1.0f),      XMFLOAT2(1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},
		{ XMFLOAT4(-1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},

		{ XMFLOAT4(1.0f, -1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},
		{ XMFLOAT4(1.0f, -1.0f, -1.0f, 1.0f),      XMFLOAT2(1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},
		{ XMFLOAT4(1.0f, 1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},
		{ XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},

		{ XMFLOAT4(-1.0f, -1.0f, -1.0f, 1.0f),     XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},
		{ XMFLOAT4(1.0f, -1.0f, -1.0f, 1.0f),      XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},
		{ XMFLOAT4(1.0f, 1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},
		{ XMFLOAT4(-1.0f, 1.0f, -1.0f, 1.0f),      XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},

		{ XMFLOAT4(-1.0f, -1.0f, 1.0f, 1.0f),      XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
		{ XMFLOAT4(1.0f, -1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
		{ XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
		{ XMFLOAT4(-1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
	};

	D3D11_BUFFER_DESC bd;    // Структура, описывающая создаваемый буфер
	ZeroMemory(&bd, sizeof(bd));                // очищаем ее
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex) * 24;    // размер буфера
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;    // тип буфера - буфер вершин
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData;    // Структура, содержащая данные буфера
	ZeroMemory(&InitData, sizeof(InitData));    // очищаем ее
	InitData.pSysMem = vertices;  // указатель на наши 8 вершин

	HRESULT hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &pCubeVertexBuffer);
	if (FAILED(hr)) return;

	// Создание буфера индексов
	// 1) cоздание массива с данными
	WORD indices[] =
	{
		0,1,3,
		3,1,2,

		5,4,6,
		6,4,7,

		8,9,11,
		11,9,10,

		13,12,14,
		14,12,15,

		16,17,19,
		19,17,18,

		21,20,22,
		22,20,23
	};

	// 2) cоздание объекта буфера
	bd.Usage = D3D11_USAGE_DEFAULT;        // Структура, описывающая создаваемый буфер
	bd.ByteWidth = sizeof(WORD) * 36;    // 36 вершин для 12 треугольников (6 сторон)
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER; // тип - буфер индексов
	bd.CPUAccessFlags = 0;

	InitData.pSysMem = indices;  // указатель на наш массив индексов

	// Вызов метода g_pd3dDevice создаст объект буфера индексов
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &pCubeIndexBuffer);
	if (FAILED(hr)) return;

	// Установка буфера вершин
	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	g_pDeviceContext->IASetVertexBuffers(0, 1, &pCubeVertexBuffer, &stride, &offset);
	g_pDeviceContext->IASetIndexBuffer(pCubeIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	g_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_pDeviceContext->DrawIndexed(36, 0, 0);

	pCubeVertexBuffer->Release();
	pCubeIndexBuffer->Release();
}
void recreate_render_target_view(int width, int height)
{
	g_pDeviceContext->OMSetRenderTargets(0, 0, 0);
	if (g_pRenderTargetView)
		g_pRenderTargetView->Release();
	if (g_pDepthStencil)
		g_pDepthStencil->Release();
	if (g_pDepthStencilView)
		g_pDepthStencilView->Release();
	HRESULT hr;
	hr = g_pSwapChain->ResizeBuffers(1, width, height, DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr)) return;
	ID3D11Texture2D* pBuffer;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBuffer);
	if (FAILED(hr)) return;

	hr = g_pd3dDevice->CreateRenderTargetView(pBuffer, NULL,
		&g_pRenderTargetView);
	if (FAILED(hr))
	{
		pBuffer->Release();
		return;
	}
	pBuffer->Release();

	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = g_SampleCount;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;

	hr = g_pd3dDevice->CreateTexture2D(&descDepth, NULL, &g_pDepthStencil);
	if (FAILED(hr)) return;

	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;

	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
	if (FAILED(hr)) return;

	g_pDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	D3D11_VIEWPORT vp;
	vp.Width = width;
	vp.Height = height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pDeviceContext->RSSetViewports(1, &vp);

	g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / (FLOAT)height, 0.01f, 100.f);
}
void update_cubemap_matrixes(const XMMATRIX& viewMatrix, const  XMMATRIX& projMatrix)
{
	ConstantBufferHDRMatrixes cb1;
	cb1.mWorld = XMMatrixTranspose(XMMatrixIdentity());
	cb1.mView = XMMatrixTranspose(viewMatrix);
	cb1.mProjection = XMMatrixTranspose(projMatrix);
	g_pDeviceContext->UpdateSubresource(g_pCubemapCBMatrixes, 0, NULL, &cb1, 0, 0);
}
void create_cubemap_texure(int width, int height, ID3D11ShaderResourceView*& cubeShaderResourceView, bool upsideDown, bool secondWay, DXGI_FORMAT format)
{
	// first way
	if (!secondWay)
	{
		ID3D11Texture2D* cubeTexture = nullptr;
		// rendering to cubemap texure
		XMFLOAT3 vectors[] =
		{
			XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(-1.0f,  0.0f,  0.0f), XMFLOAT3(0.0f, -1.0f,  0.0f),
			XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(1.0f,  0.0f,  0.0f), XMFLOAT3(0.0f, -1.0f,  0.0f),
			XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.0f,  -1.0f,  0.0f), XMFLOAT3(0.0f,  0.0f,  -1.0f),
			XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.0f, 1.0f,  0.0f), XMFLOAT3(0.0f,  0.0f, 1.0f),
			XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.0f,  0.0f,  1.0f), XMFLOAT3(0.0f, -1.0f,  0.0f),
			XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.0f,  0.0f, -1.0f), XMFLOAT3(0.0f, -1.0f,  0.0f)
		};

		if (upsideDown)
		{
			vectors[1] = XMFLOAT3(1.0f, 0.0f, 0.0f);	vectors[2] = XMFLOAT3(0.0f, 1.0f, 0.0f);
			vectors[4] = XMFLOAT3(-1.0f, 0.0f, 0.0f);	vectors[5] = XMFLOAT3(0.0f, 1.0f, 0.0f);
			vectors[7] = XMFLOAT3(0.0f, 1.0f, 0.0f);	vectors[8] = XMFLOAT3(0.0f, 0.0f, -1.0f);
			vectors[10] = XMFLOAT3(0.0f, -1.0f, 0.0f);	vectors[11] = XMFLOAT3(0.0f, 0.0f, 1.0f);
			vectors[13] = XMFLOAT3(0.0f, 0.0f, 1.0f);	vectors[14] = XMFLOAT3(0.0f, 1.0f, 0.0f);
			vectors[16] = XMFLOAT3(0.0f, 0.0f, -1.0f);	vectors[17] = XMFLOAT3(0.0f, 1.0f, 0.0f);
		}

		XMMATRIX captureViews[] =
		{
			XMMatrixLookAtLH(XMLoadFloat3(&vectors[0]), XMLoadFloat3(&vectors[1]), XMLoadFloat3(&vectors[2])),
			XMMatrixLookAtLH(XMLoadFloat3(&vectors[3]), XMLoadFloat3(&vectors[4]), XMLoadFloat3(&vectors[5])),
			XMMatrixLookAtLH(XMLoadFloat3(&vectors[6]), XMLoadFloat3(&vectors[7]), XMLoadFloat3(&vectors[8])),
			XMMatrixLookAtLH(XMLoadFloat3(&vectors[9]), XMLoadFloat3(&vectors[10]), XMLoadFloat3(&vectors[11])),
			XMMatrixLookAtLH(XMLoadFloat3(&vectors[12]), XMLoadFloat3(&vectors[13]), XMLoadFloat3(&vectors[14])),
			XMMatrixLookAtLH(XMLoadFloat3(&vectors[15]), XMLoadFloat3(&vectors[16]), XMLoadFloat3(&vectors[17]))
		};
		g_pDeviceContext->OMSetDepthStencilState(g_pHDRdepthStencilState, 1);
		g_pDeviceContext->RSSetState(g_pHDRrasterizerState);

		recreate_render_target_view(width, height);

		//Description of each face
		D3D11_TEXTURE2D_DESC texDesc = {};

		D3D11_TEXTURE2D_DESC texDesc1 = {};
		//The Shader Resource view description
		D3D11_SHADER_RESOURCE_VIEW_DESC SMViewDesc = {};

		ID3D11Texture2D* tex[6] = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
		for (int i = 0; i < 6; i++)
		{
			// clear
			g_pDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);
			float Color[4]{ 0.02f, 0.02f, 0.02f, 1.0f };
			g_pDeviceContext->ClearRenderTargetView(g_pRenderTargetView, Color);
			g_pDeviceContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.f, 0);

			// prepare
			update_cubemap_matrixes(captureViews[i], XMMatrixPerspectiveFovLH(XMConvertToRadians(90.f), 1.f, 0.1f, 10.f));

			g_pDeviceContext->VSSetConstantBuffers(0, 1, &g_pCubemapCBMatrixes);
			g_pDeviceContext->PSSetConstantBuffers(0, 1, &g_pCubemapCBMatrixes);
			g_pDeviceContext->PSSetSamplers(0, 1, &g_pSamplerLinear);

			// draw cube
			renderCube();

			g_pSwapChain->Present(1, 0);

			// copy framebuffer to texture
			ID3D11Texture2D* pBuffer;
			g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBuffer));
			if (pBuffer && tex[i] == nullptr)
			{
				D3D11_TEXTURE2D_DESC td;
				pBuffer->GetDesc(&td);
				td.Usage = D3D11_USAGE_STAGING;
				td.Width = width;
				td.Height = height;
				td.BindFlags = 0;
				td.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
				HRESULT hr = g_pd3dDevice->CreateTexture2D(&td, NULL, &tex[i]);
				if (FAILED(hr))
				{
					std::cout << "Can't create texture!\n";
					pBuffer->Release();
					g_pDeviceContext->OMSetDepthStencilState(0, 1);
					g_pDeviceContext->RSSetState(g_pRasterState);
					return;
				}
				std::cout << "Format of the texture captured by first method: " << td.Format << '\n';
			}
			g_pDeviceContext->CopyResource(tex[i], pBuffer);
			pBuffer->Release();
		}
		g_pDeviceContext->OMSetDepthStencilState(0, 1);
		g_pDeviceContext->RSSetState(g_pRasterState);

		tex[0]->GetDesc(&texDesc1);

		texDesc.Width = texDesc1.Width;
		texDesc.Height = texDesc1.Height;
		texDesc.MipLevels = texDesc1.MipLevels;
		texDesc.ArraySize = 6;
		texDesc.Format = texDesc1.Format;
		texDesc.CPUAccessFlags = 0;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

		SMViewDesc.Format = texDesc.Format;
		SMViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		SMViewDesc.TextureCube.MipLevels = texDesc.MipLevels;
		SMViewDesc.TextureCube.MostDetailedMip = 0;

		g_pd3dDevice->CreateTexture2D(&texDesc, NULL, &cubeTexture);


		for (int i = 0; i < 6; i++)
		{
			for (UINT mipLevel = 0; mipLevel < texDesc.MipLevels; ++mipLevel)
			{
				D3D11_MAPPED_SUBRESOURCE mappedTex2D;
				HRESULT hr = (g_pDeviceContext->Map(tex[i], mipLevel, D3D11_MAP_READ, 0, &mappedTex2D));
				assert(SUCCEEDED(hr));
				g_pDeviceContext->UpdateSubresource(cubeTexture, D3D11CalcSubresource(mipLevel, i, texDesc.MipLevels),
					0, mappedTex2D.pData, mappedTex2D.RowPitch, mappedTex2D.DepthPitch);

				g_pDeviceContext->Unmap(tex[i], mipLevel);

				// save textures to disc
				/*if (mipLevel == 0)
				{
					ScratchImage texture;
					CaptureTexture(g_pd3dDevice, g_pDeviceContext, tex[i], texture);
					char buffer[32];
					sprintf_s(buffer, "tex_%d.tga", i);
					std::wstring wstr{ &buffer[0], &buffer[32] };
					SaveToTGAFile(*texture.GetImage(0, 0, 0), wstr.c_str());
				}	*/
			}
		}

		HRESULT hr = g_pd3dDevice->CreateShaderResourceView(cubeTexture, &SMViewDesc, &cubeShaderResourceView);
		if (FAILED(hr))
		{
			std::cout << "Can't create cubemap shader resouce view\n";
		}

		for (int i = 0; i < 6; i++)
		{
			tex[i]->Release();
		}

	}
	// second way
	else
	{
		XMFLOAT3 vectors[] =
		{
			XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(-1.0f,  0.0f,  0.0f), XMFLOAT3(0.0f, -1.0f,  0.0f),
			XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(1.0f,  0.0f,  0.0f), XMFLOAT3(0.0f, -1.0f,  0.0f),
			XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.0f,  -1.0f,  0.0f), XMFLOAT3(0.0f,  0.0f,  -1.0f),
			XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.0f, 1.0f,  0.0f), XMFLOAT3(0.0f,  0.0f, 1.0f),
			XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.0f,  0.0f,  1.0f), XMFLOAT3(0.0f, -1.0f,  0.0f),
			XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.0f,  0.0f, -1.0f), XMFLOAT3(0.0f, -1.0f,  0.0f)
		};

		if (upsideDown)
		{
			vectors[1] = XMFLOAT3(1.0f, 0.0f, 0.0f);	vectors[2] = XMFLOAT3(0.0f, 1.0f, 0.0f);
			vectors[4] = XMFLOAT3(-1.0f, 0.0f, 0.0f);	vectors[5] = XMFLOAT3(0.0f, 1.0f, 0.0f);
			vectors[7] = XMFLOAT3(0.0f, 1.0f, 0.0f);	vectors[8] = XMFLOAT3(0.0f, 0.0f, -1.0f);
			vectors[10] = XMFLOAT3(0.0f, -1.0f, 0.0f);	vectors[11] = XMFLOAT3(0.0f, 0.0f, 1.0f);
			vectors[13] = XMFLOAT3(0.0f, 0.0f, 1.0f);	vectors[14] = XMFLOAT3(0.0f, 1.0f, 0.0f);
			vectors[16] = XMFLOAT3(0.0f, 0.0f, -1.0f);	vectors[17] = XMFLOAT3(0.0f, 1.0f, 0.0f);
		}

		XMMATRIX captureViews[] =
		{
			XMMatrixLookAtLH(XMLoadFloat3(&vectors[0]), XMLoadFloat3(&vectors[1]), XMLoadFloat3(&vectors[2])),
			XMMatrixLookAtLH(XMLoadFloat3(&vectors[3]), XMLoadFloat3(&vectors[4]), XMLoadFloat3(&vectors[5])),
			XMMatrixLookAtLH(XMLoadFloat3(&vectors[6]), XMLoadFloat3(&vectors[7]), XMLoadFloat3(&vectors[8])),
			XMMatrixLookAtLH(XMLoadFloat3(&vectors[9]), XMLoadFloat3(&vectors[10]), XMLoadFloat3(&vectors[11])),
			XMMatrixLookAtLH(XMLoadFloat3(&vectors[12]), XMLoadFloat3(&vectors[13]), XMLoadFloat3(&vectors[14])),
			XMMatrixLookAtLH(XMLoadFloat3(&vectors[15]), XMLoadFloat3(&vectors[16]), XMLoadFloat3(&vectors[17]))
		};

		g_pDeviceContext->OMSetDepthStencilState(g_pHDRdepthStencilState, 1);
		g_pDeviceContext->RSSetState(g_pHDRrasterizerState);

		std::vector<RenderTexture*> cubeFaces;

		// Create faces
		for (int i = 0; i < 6; ++i)
		{
			RenderTexture* renderTexture = new RenderTexture;
			renderTexture->Initialise(g_pd3dDevice, width, height, 1, format);
			cubeFaces.push_back(renderTexture);
		}

		for (int i = 0; i < 6; i++)
		{
			RenderTexture* texture = cubeFaces[i];
			// clear		
			recreate_render_target_view(width, height);
			texture->SetRenderTarget(g_pDeviceContext, g_pDepthStencilView);
			texture->ClearRenderTarget(g_pDeviceContext, g_pDepthStencilView, 0.0f, 0.0f, 0.0f, 1.0f);

			// prepare
			update_cubemap_matrixes(captureViews[i], XMMatrixPerspectiveFovLH(XMConvertToRadians(90.f), 1.f, 0.1f, 10.f));

			g_pDeviceContext->VSSetConstantBuffers(0, 1, &g_pCubemapCBMatrixes);
			g_pDeviceContext->PSSetConstantBuffers(0, 1, &g_pCubemapCBMatrixes);
			g_pDeviceContext->PSSetSamplers(0, 1, &g_pSamplerLinear);

			// draw cube
			renderCube();

			g_pSwapChain->Present(1, 0);
		}

		ID3D11Texture2D* cubeTexture = nullptr;

		D3D11_TEXTURE2D_DESC texDesc;
		texDesc.Width = width;
		texDesc.Height = height;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 6;
		texDesc.Format = format;
		texDesc.SampleDesc.Count = 1;
		texDesc.SampleDesc.Quality = 0;
		texDesc.Usage = D3D11_USAGE_DEFAULT;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		texDesc.CPUAccessFlags = 0;
		texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

		HRESULT result = g_pd3dDevice->CreateTexture2D(&texDesc, nullptr, &cubeTexture);
		if (FAILED(result)) return;

		D3D11_BOX sourceRegion;
		for (int i = 0; i < 6; ++i)
		{
			RenderTexture* texture = cubeFaces[i];

			sourceRegion.left = 0;
			sourceRegion.right = width;
			sourceRegion.top = 0;
			sourceRegion.bottom = height;
			sourceRegion.front = 0;
			sourceRegion.back = 1;

			g_pDeviceContext->CopySubresourceRegion(cubeTexture, D3D11CalcSubresource(0, i, 1), 0, 0, 0, texture->GetTexture(),
				0, &sourceRegion);
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		srvDesc.Format = texDesc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		srvDesc.TextureCube.MipLevels = texDesc.MipLevels;
		srvDesc.TextureCube.MostDetailedMip = 0;

		result = g_pd3dDevice->CreateShaderResourceView(cubeTexture, &srvDesc, &cubeShaderResourceView);
		if (FAILED(result)) return;

		for (RenderTexture* cubeFace : cubeFaces)
			delete cubeFace;

		g_pDeviceContext->OMSetDepthStencilState(0, 1);
		g_pDeviceContext->RSSetState(g_pRasterState);
		g_pDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);
	}
}
void update_prefilter_cubemap_matrixes(const XMMATRIX& viewMatrix, const  XMMATRIX& projMatrix, float roughness)
{
	ConstantBufferPreFilterMatrixes cb1;
	cb1.mWorld = XMMatrixTranspose(XMMatrixIdentity());
	cb1.mView = XMMatrixTranspose(viewMatrix);
	cb1.mProjection = XMMatrixTranspose(projMatrix);
	cb1.mCustomData = XMFLOAT4(roughness, 0, 0, 0);
	g_pDeviceContext->UpdateSubresource(g_pPreFilterCBMatrixes, 0, NULL, &cb1, 0, 0);
}
void create_prefilter_cubemap_texture(int width, int height, ID3D11ShaderResourceView*& cubeShaderResourceView, bool upsideDown, DXGI_FORMAT format)
{
	ID3D11Texture2D* cubeTexture = nullptr;

	D3D11_TEXTURE2D_DESC texDesc;
	texDesc.Width = width;
	texDesc.Height = height;
	texDesc.MipLevels = 5;
	texDesc.ArraySize = 6;
	texDesc.Format = format;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	HRESULT result = g_pd3dDevice->CreateTexture2D(&texDesc, nullptr, &cubeTexture);
	if (FAILED(result)) return;

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = texDesc.MipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;

	result = g_pd3dDevice->CreateShaderResourceView(cubeTexture, &srvDesc, &g_pPreFilterShaderResourceView);
	if (FAILED(result)) return;

	XMFLOAT3 vectors[] =
	{
		XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(-1.0f,  0.0f,  0.0f), XMFLOAT3(0.0f, -1.0f,  0.0f),
		XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(1.0f,  0.0f,  0.0f), XMFLOAT3(0.0f, -1.0f,  0.0f),
		XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.0f,  -1.0f,  0.0f), XMFLOAT3(0.0f,  0.0f,  -1.0f),
		XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.0f, 1.0f,  0.0f), XMFLOAT3(0.0f,  0.0f, 1.0f),
		XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.0f,  0.0f,  1.0f), XMFLOAT3(0.0f, -1.0f,  0.0f),
		XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.0f,  0.0f, -1.0f), XMFLOAT3(0.0f, -1.0f,  0.0f)
	};

	if (upsideDown)
	{
		vectors[1] = XMFLOAT3(1.0f, 0.0f, 0.0f);	vectors[2] = XMFLOAT3(0.0f, 1.0f, 0.0f);
		vectors[4] = XMFLOAT3(-1.0f, 0.0f, 0.0f);	vectors[5] = XMFLOAT3(0.0f, 1.0f, 0.0f);
		vectors[7] = XMFLOAT3(0.0f, 1.0f, 0.0f);	vectors[8] = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vectors[10] = XMFLOAT3(0.0f, -1.0f, 0.0f);	vectors[11] = XMFLOAT3(0.0f, 0.0f, 1.0f);
		vectors[13] = XMFLOAT3(0.0f, 0.0f, 1.0f);	vectors[14] = XMFLOAT3(0.0f, 1.0f, 0.0f);
		vectors[16] = XMFLOAT3(0.0f, 0.0f, -1.0f);	vectors[17] = XMFLOAT3(0.0f, 1.0f, 0.0f);
	}

	XMMATRIX captureViews[] =
	{
		XMMatrixLookAtLH(XMLoadFloat3(&vectors[0]), XMLoadFloat3(&vectors[1]), XMLoadFloat3(&vectors[2])),
		XMMatrixLookAtLH(XMLoadFloat3(&vectors[3]), XMLoadFloat3(&vectors[4]), XMLoadFloat3(&vectors[5])),
		XMMatrixLookAtLH(XMLoadFloat3(&vectors[6]), XMLoadFloat3(&vectors[7]), XMLoadFloat3(&vectors[8])),
		XMMatrixLookAtLH(XMLoadFloat3(&vectors[9]), XMLoadFloat3(&vectors[10]), XMLoadFloat3(&vectors[11])),
		XMMatrixLookAtLH(XMLoadFloat3(&vectors[12]), XMLoadFloat3(&vectors[13]), XMLoadFloat3(&vectors[14])),
		XMMatrixLookAtLH(XMLoadFloat3(&vectors[15]), XMLoadFloat3(&vectors[16]), XMLoadFloat3(&vectors[17]))
	};

	g_pDeviceContext->OMSetDepthStencilState(g_pHDRdepthStencilState, 1);
	g_pDeviceContext->RSSetState(g_pHDRrasterizerState);

	std::vector<RenderTexture*> cubeFaces;

	// Create faces
	for (int i = 0; i < 6; ++i)
	{
		RenderTexture* renderTexture = new RenderTexture;
		renderTexture->Initialise(g_pd3dDevice, width, height, 1, format);
		cubeFaces.push_back(renderTexture);
	}

	unsigned int maxMipLevels = 5;
	for (int mip = 0; mip < maxMipLevels; ++mip)
	{
		int preFilterSize = width;
		const unsigned int mipWidth = unsigned int(preFilterSize * pow(0.5, mip));
		const unsigned int mipHeight = unsigned int(preFilterSize * pow(0.5, mip));


		for (int i = 0; i < 6; ++i)
		{
			delete cubeFaces[i];
			RenderTexture* renderTexture = new RenderTexture;
			renderTexture->Initialise(g_pd3dDevice, mipWidth, mipHeight, 1, format);
			cubeFaces[i] = renderTexture;
		}

		for (int i = 0; i < 6; i++)
		{
			RenderTexture* texture = cubeFaces[i];
			// clear		
			recreate_render_target_view(mipWidth, mipHeight);
			texture->SetRenderTarget(g_pDeviceContext, g_pDepthStencilView);
			texture->ClearRenderTarget(g_pDeviceContext, g_pDepthStencilView, 0.0f, 0.0f, 0.0f, 1.0f);

			// prepare
			const float roughness = float(mip) / 4.0f;
			update_prefilter_cubemap_matrixes(captureViews[i], XMMatrixPerspectiveFovLH(XMConvertToRadians(90.f), 1.f, 0.1f, 10.f), roughness);

			g_pDeviceContext->VSSetConstantBuffers(0, 1, &g_pPreFilterCBMatrixes);
			g_pDeviceContext->PSSetConstantBuffers(0, 1, &g_pPreFilterCBMatrixes);
			g_pDeviceContext->PSSetSamplers(0, 1, &g_pSamplerLinear);

			// draw cube
			renderCube();

			g_pSwapChain->Present(1, 0);
		}

		D3D11_BOX sourceRegion;
		for (int i = 0; i < 6; ++i)
		{
			RenderTexture* texture = cubeFaces[i];

			sourceRegion.left = 0;
			sourceRegion.right = mipWidth;
			sourceRegion.top = 0;
			sourceRegion.bottom = mipHeight;
			sourceRegion.front = 0;
			sourceRegion.back = 1;

			g_pDeviceContext->CopySubresourceRegion(cubeTexture, D3D11CalcSubresource(mip, i, 5), 0, 0, 0, texture->GetTexture(),
				0, &sourceRegion);
		}
	}

	for (int i = 0; i < 6; ++i)
		delete cubeFaces[i];

	g_pDeviceContext->OMSetDepthStencilState(0, 1);
	g_pDeviceContext->RSSetState(g_pRasterState);
	g_pDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);
}
void create_brdf_texture()
{
	int width = 512, height = 512;
	RenderTexture* texture = new RenderTexture;
	texture->Initialise(g_pd3dDevice, width, height, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	recreate_render_target_view(width, height);
	texture->SetRenderTarget(g_pDeviceContext, g_pDepthStencilView);
	texture->ClearRenderTarget(g_pDeviceContext, g_pDepthStencilView, 0.0f, 0.0f, 0.0f, 1.0f);

	g_pDeviceContext->OMSetDepthStencilState(g_pHDRdepthStencilState, 1);
	g_pDeviceContext->RSSetState(g_pHDRrasterizerState);

	XMFLOAT3 vectors[] =
	{
		XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT3(0.0f,  0.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f),
	};
	XMMATRIX viewMatrix = XMMatrixLookAtLH(XMLoadFloat3(&vectors[0]), XMLoadFloat3(&vectors[1]), XMLoadFloat3(&vectors[2]));
	XMMATRIX projMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(90.f), 1.f, 0.1f, 100.f);
	//XMMATRIX projMatrix = XMMatrixOrthographicLH(2, 2, 0.1, 10);
	update_prefilter_cubemap_matrixes(viewMatrix, projMatrix, 0.f);

	g_pDeviceContext->VSSetConstantBuffers(0, 1, &g_pPreFilterCBMatrixes);
	g_pDeviceContext->PSSetConstantBuffers(0, 1, &g_pPreFilterCBMatrixes);
	g_pDeviceContext->PSSetSamplers(0, 1, &g_pSamplerLinear);

	renderCube();

	g_pSwapChain->Present(1, 0);

	ScratchImage image;
	HRESULT hr = CaptureTexture(g_pd3dDevice, g_pDeviceContext, texture->GetTexture(), image);
	if (FAILED(hr)) std::cout << "Can't capture texture\n";
	//hr = SaveToTGAFile(*image.GetImage(0, 0, 0), L"brdf_lut.tga");
	hr = SaveToWICFile(*image.GetImage(0, 0, 0), WIC_FLAGS_NONE, GetWICCodec(WIC_CODEC_PNG), L"brdf_lut.png");
	if (FAILED(hr)) std::cout << "Can't save TGA file\n";

	// copy result to shader resource view
	D3D11_TEXTURE2D_DESC textureDesc;
	ZeroMemory(&textureDesc, sizeof(textureDesc));

	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;

	HRESULT result = g_pd3dDevice->CreateTexture2D(&textureDesc, nullptr, &g_pBRDFTexture);
	if (FAILED(result)) return;

	ID3D11Resource* srcResource;
	ID3D11Texture2D* srcTex;
	texture->GetSRV()->GetResource(&srcResource);
	srcTex = (ID3D11Texture2D*)srcResource;
	g_pDeviceContext->CopyResource(g_pBRDFTexture, srcTex);

	if (g_pBRDFTexture)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC shaderResourceViewDesc;
		shaderResourceViewDesc.Format = textureDesc.Format;
		shaderResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		shaderResourceViewDesc.Texture2D.MostDetailedMip = 0;
		shaderResourceViewDesc.Texture2D.MipLevels = 1;
		g_pd3dDevice->CreateShaderResourceView(g_pBRDFTexture, &shaderResourceViewDesc, &g_pBRDFShaderResourceView);
	}
	delete texture;

	g_pDeviceContext->OMSetDepthStencilState(0, 1);
	g_pDeviceContext->RSSetState(g_pRasterState);

	/*HRESULT hr = CreateWICTextureFromFile(g_pd3dDevice, L"ibl_brdf_lut.png", nullptr, &g_pBRDFShaderResourceView);
	if (FAILED(hr)) std::cout << "Can't load BRDF LUT texture from file\n";
	ID3D11Resource* resource;
	g_pBRDFShaderResourceView->GetResource(&resource);
	ID3D11Texture2D* texture = (ID3D11Texture2D*)resource;
	D3D11_TEXTURE2D_DESC desc;
	texture->GetDesc(&desc);
	std::cout << "BRDF LUT texture format: " << desc.Format << '\n';*/
}
void init_hdr_cubemap(std::string path)
{
	HRESULT hr;
	auto image = std::make_unique<ScratchImage>();
	std::wstring wPath{ path.begin(), path.end() };
	hr = LoadFromHDRFile(wPath.c_str(), nullptr, *image);
	if (FAILED(hr))
	{
		std::cout << "Can't load HDR texture\n";
		return;
	}

	hr = CreateShaderResourceView(g_pd3dDevice, (*image).GetImages(), (*image).GetImageCount(),
		(*image).GetMetadata(), &g_pHDRShaderResourceView);
	if (FAILED(hr))
	{
		std::cout << "Can't create SRV from HDR texture\n";
		return;
	}

	std::cout << "HDRI image format : " << image->GetMetadata().format << '\n';

	// init constant buffers
	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ConstantBufferHDRMatrixes);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pCubemapCBMatrixes);
	if (FAILED(hr)) return;

	bd.ByteWidth = sizeof(ConstantBufferPreFilterMatrixes);
	hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pPreFilterCBMatrixes);
	if (FAILED(hr)) return;


	D3D11_RASTERIZER_DESC rdesc;
	ZeroMemory(&rdesc, sizeof(rdesc));
	rdesc.CullMode = D3D11_CULL_NONE;
	rdesc.FillMode = D3D11_FILL_SOLID;
	hr = g_pd3dDevice->CreateRasterizerState(&rdesc, &g_pHDRrasterizerState);

	D3D11_DEPTH_STENCIL_DESC dsdesc;
	ZeroMemory(&dsdesc, sizeof(dsdesc));
	dsdesc.DepthEnable = true;
	dsdesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	hr = g_pd3dDevice->CreateDepthStencilState(&dsdesc, &g_pHDRdepthStencilState);
	if (FAILED(hr)) std::cout << "Can't create DepthStencilState!\n";


	// Loading rect to cubemap shaders
	load_shader("EquirectangularToCubemap.shader", "EquirectangularToCubemap.shader", g_pRectToCubeVertexShader, g_pRectToCubePixelShader);

	// loading cubemap shaders
	load_shader("Cubemap.shader", "Cubemap.shader", g_pCubemapVertexShader, g_pCubemapPixelShader);
	
	// loading irradiance shaders
	load_shader("Irradiance.shader", "Irradiance.shader", g_pIrradianceVertexShader, g_pIrradiancePixelShader);

	// loading prefilter shaders
	load_shader("PreFilter.shader", "PreFilter.shader", g_pPreFilterVertexShader, g_pPreFilterPixelShader);

	// loading integrate BRDF shaders
	load_shader("IntegrateBRDF.shader", "IntegrateBRDF.shader", g_pBRDFVertexShader, g_pBRDFPixelShader);


	g_pDeviceContext->VSSetShader(g_pRectToCubeVertexShader, NULL, 0);
	g_pDeviceContext->PSSetShader(g_pRectToCubePixelShader, NULL, 0);
	g_pDeviceContext->PSSetShaderResources(0, 1, &g_pHDRShaderResourceView);
	create_cubemap_texure(512, 512, g_pCubemapShaderResourceView, false, true, DXGI_FORMAT_R16G16B16A16_FLOAT);

	g_pDeviceContext->VSSetShader(g_pIrradianceVertexShader, NULL, 0);
	g_pDeviceContext->PSSetShader(g_pIrradiancePixelShader, NULL, 0);
	g_pDeviceContext->PSSetShaderResources(0, 1, &g_pCubemapShaderResourceView);
	create_cubemap_texure(512, 512, g_pIrradianceShaderResourceView, true, true, DXGI_FORMAT_R16G16B16A16_FLOAT);

	g_pDeviceContext->VSSetShader(g_pPreFilterVertexShader, NULL, 0);
	g_pDeviceContext->PSSetShader(g_pPreFilterPixelShader, NULL, 0);
	g_pDeviceContext->PSSetShaderResources(0, 1, &g_pCubemapShaderResourceView);
	create_prefilter_cubemap_texture(256, 256, g_pPreFilterShaderResourceView, true, DXGI_FORMAT_R16G16B16A16_FLOAT);

	g_pDeviceContext->VSSetShader(g_pBRDFVertexShader, NULL, 0);
	g_pDeviceContext->PSSetShader(g_pBRDFPixelShader, NULL, 0);
	create_brdf_texture();
}
void render_cubemap(ID3D11VertexShader* cubemapVS, ID3D11PixelShader* cubemapPS, ID3D11ShaderResourceView*& cubemapSRV)
{
	update_cubemap_matrixes(g_View, g_Projection);

	g_pDeviceContext->VSSetShader(cubemapVS, NULL, 0);
	g_pDeviceContext->VSSetConstantBuffers(0, 1, &g_pCubemapCBMatrixes);
	g_pDeviceContext->PSSetConstantBuffers(0, 1, &g_pCubemapCBMatrixes);
	g_pDeviceContext->PSSetSamplers(0, 1, &g_pSamplerLinear);

	g_pDeviceContext->PSSetShader(cubemapPS, NULL, 0);
	g_pDeviceContext->PSSetShaderResources(0, 1, &cubemapSRV);

	g_pDeviceContext->OMSetDepthStencilState(g_pHDRdepthStencilState, 1);
	g_pDeviceContext->RSSetState(g_pHDRrasterizerState);

	renderCube();

	g_pDeviceContext->OMSetDepthStencilState(0, 1);
	g_pDeviceContext->RSSetState(g_pRasterState);
}


void FramebufferSizeCallback(GLFWwindow* window, int width, int height)
{
	recreate_render_target_view(width, height);
}
void MouseCallback(GLFWwindow* window, double xposIn, double yposIn)
{
	float xpos = static_cast<float>(xposIn);
	float ypos = static_cast<float>(yposIn);

	static bool firstMouse = true;
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	if (viewPortActive)
	{
		camera.ProcessMouseMovement(xoffset, yoffset);
	}
}
void ProcessInput(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		exit(0);

	static bool keyPressed = false;
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(CameraMovement::FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(CameraMovement::BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(CameraMovement::LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(CameraMovement::RIGHT, deltaTime);

	if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !keyPressed)
	{
		viewPortActive = !viewPortActive;
		keyPressed = true;
	}
	else if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE)
		keyPressed = false;
}

int main()
{
	//HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	//if (FAILED(hr)) return hr;

	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_SAMPLES, g_SampleCount);
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "DirectX11", NULL, NULL);
	glfwMakeContextCurrent(window);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	g_pWindow = window;
	g_hWnd = glfwGetWin32Window(window);

	glfwSetFramebufferSizeCallback(window, &FramebufferSizeCallback);
	glfwSetCursorPosCallback(window, &MouseCallback);

	if (FAILED(InitDeivce()))
	{
		CleanupDevice();
		return 0;
	}

	if (FAILED(InitGeometry()))
	{
		CleanupDevice();
		return 0;
	}

	if (FAILED(InitMatrixes()))
	{
		CleanupDevice();
		return 0;
	}

	//init_hdr_cubemap("resources/goegap_road_8k.hdr");
	//init_hdr_cubemap("resources/illovo_beach_balcony_8k.hdr");
	//init_hdr_cubemap("resources/cobblestone_street_night_8k.hdr");	
	//init_hdr_cubemap("resources/zwartkops_curve_afternoon_8k.hdr");
	//init_hdr_cubemap("resources/golden_bay_8k.hdr");
	//init_hdr_cubemap("resources/studio_small_09_8k.hdr");
	//init_hdr_cubemap("resources/brown_photostudio_02_8k.hdr");
	init_hdr_cubemap("resources/brown_photostudio_06_8k.hdr");

	std::wstring prefix = L"resources/colt_python/";
	model = new Model(g_pd3dDevice, "resources/colt_python/source/revolver_game.fbx", prefix + L"textures/M_WP_Revolver_albedo.jpg", prefix + L"textures/M_WP_Revolver_normal.png", prefix + L"textures/M_WP_Revolver_roughness.jpg", prefix + L"textures/M_WP_Revolver_metallic.jpg");

	//std::wstring prefix = L"resources/cerberus/";
	//model = new(buffer) Model(g_pd3dDevice, "resources/cerberus/Cerberus_LP.FBX", prefix + L"textures/Cerberus_A.tga", prefix + L"textures/Cerberus_N.tga", prefix + L"textures/Cerberus_R.tga", prefix + L"textures/Cerberus_M.tga");

	ImguiInit();

	while (!glfwWindowShouldClose(window))
	{
		if (viewPortActive)
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
		else
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

		// per-frame time logic
		// --------------------
		float currentFrame = static_cast<float>(glfwGetTime());
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// input
		// -----
		ProcessInput(window);

		g_View = camera.GetViewMatrix();

		ImguiFrame();
		ImGuizmo::BeginFrame();
		ImGuizmo::Enable(g_ImguizmoEnabled);
		{
			ImGui::Begin("Object Controls");

			static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE);
			if (ImGui::IsKeyPressed(ImGuiKey_W))
				mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
			if (ImGui::IsKeyPressed(ImGuiKey_E))
				mCurrentGizmoOperation = ImGuizmo::ROTATE;
			if (ImGui::IsKeyPressed(ImGuiKey_R))
				mCurrentGizmoOperation = ImGuizmo::SCALE;

			XMFLOAT4X4 view;
			XMStoreFloat4x4(&view, g_View);
			XMFLOAT4X4 proj;
			XMStoreFloat4x4(&proj, g_Projection);
			XMFLOAT4X4 world;
			XMStoreFloat4x4(&world, g_modelMatrix);

			ImGuizmo::DecomposeMatrixToComponents(world.m[0], matrixTranslation, matrixRotation, matrixScale);

			ImGui::SliderFloat3("Tr", matrixTranslation, -5, 5);
			ImGui::SliderFloat3("Rt", matrixRotation, -180, 180);
			ImGui::SliderFloat("Sc", &g_modelScale, 0.01f, 20.0f);
			ImGui::Checkbox("Imguizmo Enabled", &g_ImguizmoEnabled);

			matrixScale[0] = matrixScale[1] = matrixScale[2] = g_modelScale;

			ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, world.m[0]);

			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
			RECT rect;
			GetClientRect(g_hWnd, &rect);
			ImGuizmo::SetRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);

			ImGuizmo::Manipulate(view.m[0], proj.m[0], mCurrentGizmoOperation,
				ImGuizmo::MODE::LOCAL, world.m[0]);

			g_modelMatrix = XMLoadFloat4x4(&world);

			if (ImGui::Button("M"))
			{
				openFile(modelPath);
				if (!modelPath.empty())
				{
					model->~Model();
					model = new(modelBuffer) Model(g_pd3dDevice, modelPath, L"", L"", L"", L"");
				}
			}
			ImGui::SameLine();
			ImGui::InputText("Model", &modelPath);

			if (ImGui::Button("1"))
			{
				openFile(albedoPath);
				if (!albedoPath.empty())
					model->changeTexture(g_pd3dDevice, g_pDeviceContext, albedoPath, Model::ALBEDO);
			}
			ImGui::SameLine();
			ImGui::InputText("Albedo", &albedoPath);

			if (ImGui::Button("2"))
			{
				openFile(normalPath);
				if (!normalPath.empty())
					model->changeTexture(g_pd3dDevice, g_pDeviceContext, normalPath, Model::NORMAL);
			}
			ImGui::SameLine();
			ImGui::InputText("Normal", &normalPath);

			if (ImGui::Button("3"))
			{
				openFile(roughnessPath);
				if (!roughnessPath.empty())
					model->changeTexture(g_pd3dDevice, g_pDeviceContext, roughnessPath, Model::ROUGHNESS);
			}
			ImGui::SameLine();
			ImGui::InputText("Roughness", &roughnessPath);

			if (ImGui::Button("4"))
			{
				openFile(metallicPath);
				if (!metallicPath.empty())
					model->changeTexture(g_pd3dDevice, g_pDeviceContext, metallicPath, Model::METALLIC);
			}
			ImGui::SameLine();
			ImGui::InputText("Metallic", &metallicPath);

			ImGui::End();
		}

		Render();

		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	ImguiCleanup();
	CleanupDevice();
	glfwTerminate();
}

HRESULT InitDeivce()
{
	HRESULT hr = S_OK;

	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	UINT createDeviceFlags = 0;
#if defined(DEBUG) || defined (_DEBUG)
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_DRIVER_TYPE driverTypes[] =
	{
		D3D_DRIVER_TYPE_HARDWARE,
		D3D_DRIVER_TYPE_WARP,
		D3D_DRIVER_TYPE_REFERENCE
	};
	UINT numDriverTypes = ARRAYSIZE(driverTypes);

	D3D_FEATURE_LEVEL featureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	UINT numFeatureLevels = ARRAYSIZE(featureLevels);

	IDXGIFactory* factory;
	IDXGIAdapter* adapter;
	IDXGIOutput* adapterOutput;
	DXGI_MODE_DESC* displayModeList;
	unsigned int numModes, numerator, denominator;

	// Create a DirectX graphics interface factory.
	hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
	if (FAILED(hr)) return hr;

	// Use the factory to create an adapter for the primary graphics interface (video card).
	hr = factory->EnumAdapters(0, &adapter);
	if (FAILED(hr)) return hr;

	// Enumerate the primary adapter output (monitor).
	hr = adapter->EnumOutputs(0, &adapterOutput);
	if (FAILED(hr)) return hr;

	// Get the number of modes that fit the DXGI_FORMAT_R8G8B8A8_UNORM display format for the adapter output (monitor).
	hr = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, NULL);
	if (FAILED(hr)) return hr;

	// Create a list to hold all the possible display modes for this monitor/video card combination.
	displayModeList = new DXGI_MODE_DESC[numModes];
	if (FAILED(hr)) return hr;

	// Now fill the display mode list structures.
	hr = adapterOutput->GetDisplayModeList(DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_ENUM_MODES_INTERLACED, &numModes, displayModeList);
	if (FAILED(hr)) return hr;

	// Now go through all the display modes and find the one that matches the screen width and height.
	// When a match is found store the numerator and denominator of the refresh rate for that monitor.
	for (int i = 0; i < numModes; i++)
	{
		if (displayModeList[i].Width == (unsigned int)width)
		{
			if (displayModeList[i].Height == (unsigned int)height)
			{
				numerator = displayModeList[i].RefreshRate.Numerator;
				denominator = displayModeList[i].RefreshRate.Denominator;
			}
		}
	}

	// Release the display mode list.
	delete[] displayModeList;
	displayModeList = 0;

	// Release the adapter output.
	adapterOutput->Release();
	adapterOutput = 0;



	D3D11CreateDevice(adapter, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels, D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pDeviceContext);
	UINT formatSupport = 0;
	hr = g_pd3dDevice->CheckFormatSupport(DXGI_FORMAT_R8G8B8A8_UNORM, &formatSupport);
	if (FAILED(hr)) return hr;

	UINT flags = D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE
		| D3D11_FORMAT_SUPPORT_MULTISAMPLE_RENDERTARGET;

	if ((formatSupport & flags) != flags)
	{
		std::cout << "Device doesn't support these formats\n";
	}

	int m_sampleCount = 0;
	UINT levels = 0;
	for (m_sampleCount = D3D11_MAX_MULTISAMPLE_SAMPLE_COUNT;
		m_sampleCount > 1; m_sampleCount--)
	{
		levels = 0;
		if (FAILED(g_pd3dDevice->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM,
			m_sampleCount, &levels)))
			continue;

		if (levels > 0)
			break;
	}

	std::cout << "Device supports " << m_sampleCount << " sample count and " << levels << " quality levels\n";

	if (m_sampleCount < 2)
	{
		std::cout << "Device doesn't support MSAA\n";
	}



	// Release the adapter.
	adapter->Release();
	adapter = 0;

	// Release the factory.
	factory->Release();
	factory = 0;

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = numerator;
	sd.BufferDesc.RefreshRate.Denominator = denominator;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = g_hWnd;
	sd.SampleDesc.Count = g_SampleCount;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	//sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

	for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
	{
		g_driverType = driverTypes[driverTypeIndex];
		hr = D3D11CreateDeviceAndSwapChain(NULL, g_driverType, NULL, createDeviceFlags, featureLevels,
			numFeatureLevels, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pDeviceContext);
		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr)) return hr;

	ID3D11Texture2D* pBackBuffer;
	hr = g_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer);
	if (FAILED(hr)) return hr;

	hr = g_pd3dDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
	pBackBuffer->Release();
	if (FAILED(hr)) return hr;

	D3D11_TEXTURE2D_DESC descDepth;
	ZeroMemory(&descDepth, sizeof(descDepth));
	descDepth.Width = width;
	descDepth.Height = height;
	descDepth.MipLevels = 1;
	descDepth.ArraySize = 1;
	descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	descDepth.SampleDesc.Count = g_SampleCount;
	descDepth.SampleDesc.Quality = 0;
	descDepth.Usage = D3D11_USAGE_DEFAULT;
	descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	descDepth.CPUAccessFlags = 0;
	descDepth.MiscFlags = 0;

	hr = g_pd3dDevice->CreateTexture2D(&descDepth, NULL, &g_pDepthStencil);
	if (FAILED(hr)) return hr;

	D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
	ZeroMemory(&descDSV, sizeof(descDSV));
	descDSV.Format = descDepth.Format;
	descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	descDSV.Texture2D.MipSlice = 0;

	hr = g_pd3dDevice->CreateDepthStencilView(g_pDepthStencil, &descDSV, &g_pDepthStencilView);
	if (FAILED(hr)) return hr;

	g_pDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);

	D3D11_RASTERIZER_DESC rasterDesc;
	// Setup the raster description which will determine how and what polygons will be drawn.
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_BACK;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;

	// Create the rasterizer state from the description we just filled out.
	hr = g_pd3dDevice->CreateRasterizerState(&rasterDesc, &g_pRasterState);
	if (FAILED(hr)) return hr;

	// Now set the rasterizer state.
	g_pDeviceContext->RSSetState(g_pRasterState);

	D3D11_VIEWPORT vp;
	vp.Width = (FLOAT)width;
	vp.Height = (FLOAT)height;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	g_pDeviceContext->RSSetViewports(1, &vp);

	return S_OK;
}

void CleanupDevice()
{
	if (g_pSamplerLinear) g_pSamplerLinear->Release();
	if (g_pCBMatrixes) g_pCBMatrixes->Release();
	if (g_pCBLight) g_pCBLight->Release();
	if (g_pIndexBuffer) g_pIndexBuffer->Release();
	if (g_pVertexBuffer) g_pVertexBuffer->Release();
	if (g_pInputLayout) g_pInputLayout->Release();
	if (g_pVertexShader) g_pVertexShader->Release();
	if (g_pPixelShader) g_pPixelShader->Release();
	if (g_pDepthStencilView) g_pDepthStencilView->Release();
	if (g_pDepthStencil) g_pDepthStencil->Release();
	if (g_pRenderTargetView) g_pRenderTargetView->Release();
	if (g_pSwapChain) g_pSwapChain->Release();
	if (g_pDeviceContext) g_pDeviceContext->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();

#if defined(DEBUG) || defined(_DEBUG)
	ID3D11Debug* debugDevice = nullptr;
	HRESULT hr = g_pd3dDevice->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void**>(&debugDevice));

	hr = debugDevice->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY);

	debugDevice->Release();
#endif
}

HRESULT InitGeometry()
{
	HRESULT hr = S_OK;

	ID3DBlob* pVSBlob = NULL;

	hr = CompileShaderFromFile(TEXT("PBR.shader"), "VSMain", "vs_5_0", &pVSBlob);
	if (FAILED(hr))
	{
		if (pVSBlob) pVSBlob->Release();
		MessageBox(g_hWnd, TEXT("Can't compile file shader.fx VS"), TEXT("Error"), MB_OK);
		return hr;
	}

	hr = g_pd3dDevice->CreateVertexShader(pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pVertexShader);
	if (FAILED(hr))
	{
		pVSBlob->Release();
		return hr;
	}

	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	UINT numElements = ARRAYSIZE(layout);
	hr = g_pd3dDevice->CreateInputLayout(layout, numElements, pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), &g_pInputLayout);
	pVSBlob->Release();
	if (FAILED(hr)) return hr;

	g_pDeviceContext->IASetInputLayout(g_pInputLayout);

	ID3DBlob* pPSBlob = NULL;

	hr = CompileShaderFromFile(TEXT("PBR.shader"), "PSMain", "ps_5_0", &pPSBlob);
	if (FAILED(hr))
	{
		if (pPSBlob) pPSBlob->Release();
		MessageBox(g_hWnd, TEXT("Can't compile file shader.fx PS"), TEXT("Error"), MB_OK);
		return hr;
	}

	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShader);
	if (FAILED(hr)) return hr;

	pPSBlob = NULL;
	hr = CompileShaderFromFile(TEXT("PBR.shader"), "PSSolid", "ps_5_0", &pPSBlob);
	if (FAILED(hr))
	{
		if (pPSBlob) pPSBlob->Release();
		MessageBox(g_hWnd, TEXT("Can't compile file shader.fx PSSolid"), TEXT("Error"), MB_OK);
		return hr;
	}
	hr = g_pd3dDevice->CreatePixelShader(pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pPixelShaderSolid);
	if (FAILED(hr)) return hr;

	// Создание буфера вершин (по 4 точки на каждую сторону куба, всего 24 вершины)

	Vertex vertices[] =
	{    /* координаты X, Y, Z            координаты текстры tu, tv   нормаль X, Y, Z        */
		{ XMFLOAT4(-1.0f, 1.0f, -1.0f, 1.0f),      XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},
		{ XMFLOAT4(1.0f, 1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},
		{ XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},
		{ XMFLOAT4(-1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},

		{ XMFLOAT4(-1.0f, -1.0f, -1.0f, 1.0f),     XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},
		{ XMFLOAT4(1.0f, -1.0f, -1.0f, 1.0f),      XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},
		{ XMFLOAT4(1.0f, -1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},
		{ XMFLOAT4(-1.0f, -1.0f, 1.0f, 1.0f),      XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, -1.0f, 0.0f)},

		{ XMFLOAT4(-1.0f, -1.0f, 1.0f, 1.0f),      XMFLOAT2(0.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},
		{ XMFLOAT4(-1.0f, -1.0f, -1.0f, 1.0f),     XMFLOAT2(1.0f, 0.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},
		{ XMFLOAT4(-1.0f, 1.0f, -1.0f, 1.0f),      XMFLOAT2(1.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},
		{ XMFLOAT4(-1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(-1.0f, 0.0f, 0.0f)},

		{ XMFLOAT4(1.0f, -1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},
		{ XMFLOAT4(1.0f, -1.0f, -1.0f, 1.0f),      XMFLOAT2(1.0f, 0.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},
		{ XMFLOAT4(1.0f, 1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},
		{ XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},

		{ XMFLOAT4(-1.0f, -1.0f, -1.0f, 1.0f),     XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},
		{ XMFLOAT4(1.0f, -1.0f, -1.0f, 1.0f),      XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},
		{ XMFLOAT4(1.0f, 1.0f, -1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},
		{ XMFLOAT4(-1.0f, 1.0f, -1.0f, 1.0f),      XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, -1.0f)},

		{ XMFLOAT4(-1.0f, -1.0f, 1.0f, 1.0f),      XMFLOAT2(0.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
		{ XMFLOAT4(1.0f, -1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 0.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
		{ XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
		{ XMFLOAT4(-1.0f, 1.0f, 1.0f, 1.0f), XMFLOAT2(0.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},
	};

	D3D11_BUFFER_DESC bd;    // Структура, описывающая создаваемый буфер
	ZeroMemory(&bd, sizeof(bd));                // очищаем ее
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Vertex) * 24;    // размер буфера
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;    // тип буфера - буфер вершин
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA InitData;    // Структура, содержащая данные буфера
	ZeroMemory(&InitData, sizeof(InitData));    // очищаем ее
	InitData.pSysMem = vertices;  // указатель на наши 8 вершин

	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pVertexBuffer);
	if (FAILED(hr)) return hr;

	// Создание буфера индексов
	// 1) cоздание массива с данными
	WORD indices[] =
	{
		3,1,0,
		2,1,3,

		6,4,5,
		7,4,6,

		11,9,8,
		10,9,11,

		14,12,13,
		15,12,14,

		19,17,16,
		18,17,19,

		22,20,21,
		23,20,22
	};

	// 2) cоздание объекта буфера
	bd.Usage = D3D11_USAGE_DEFAULT;        // Структура, описывающая создаваемый буфер
	bd.ByteWidth = sizeof(WORD) * 36;    // 36 вершин для 12 треугольников (6 сторон)
	bd.BindFlags = D3D11_BIND_INDEX_BUFFER; // тип - буфер индексов
	bd.CPUAccessFlags = 0;

	InitData.pSysMem = indices;  // указатель на наш массив индексов

	// Вызов метода g_pd3dDevice создаст объект буфера индексов
	hr = g_pd3dDevice->CreateBuffer(&bd, &InitData, &g_pIndexBuffer);
	if (FAILED(hr)) return hr;

	// Установка буфера вершин
	UINT stride = sizeof(Vertex);
	UINT offset = 0;

	g_pDeviceContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	// Установка буфера индексов
	g_pDeviceContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	// Установка способа отрисовки вершин в буфере
	g_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ConstantBufferMatrixes);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pCBMatrixes);
	if (FAILED(hr)) return hr;

	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(ConstantBufferLight);
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;
	hr = g_pd3dDevice->CreateBuffer(&bd, NULL, &g_pCBLight);
	if (FAILED(hr)) return hr;


	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Создаем интерфейс сэмпла текстурирования
	hr = g_pd3dDevice->CreateSamplerState(&sampDesc, &g_pSamplerLinear);
	if (FAILED(hr)) return hr;

	return S_OK;
}

HRESULT InitMatrixes()
{
	RECT rc;
	GetClientRect(g_hWnd, &rc);
	UINT width = rc.right - rc.left;
	UINT height = rc.bottom - rc.top;

	g_World = XMMatrixIdentity();

	//XMVECTOR Eye = XMVectorSet(0.f, 0.f, -2.f, 0.f);
	//XMVECTOR At = XMVectorSet(0.f, 0.f, 1.f, 0.f);
	//XMVECTOR Up = XMVectorSet(0.f, 1.f, 0.f, 0.f);
	g_View = camera.GetViewMatrix();

	g_Projection = XMMatrixPerspectiveFovLH(XM_PIDIV4, width / (FLOAT)height, 0.01f, 100.f);

	return S_OK;
}

void UpdateLight()
{
	if (g_driverType == D3D_DRIVER_TYPE_REFERENCE)
	{
		t += (float)XM_PI * 0.0125f;
	}
	else
	{
		static DWORD dwTimeStart = 0;
		DWORD dwTimeCur = GetTickCount();
		if (dwTimeStart == 0)
			dwTimeStart = dwTimeCur;
		t = (dwTimeCur - dwTimeStart) / 1000.0f;
	}

	vLightDirs[0] = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);
	vLightDirs[1] = XMFLOAT4(1.0f, -1.0f, 1.0f, 1.0f);
	float lightComponent = 10.f;
	vLightColors[0] = XMFLOAT4(lightComponent, lightComponent, lightComponent - 0.1f, 1.0f);
	vLightColors[1] = XMFLOAT4(lightComponent, lightComponent, lightComponent, 1.0f);

	// При помощи трансформаций поворачиваем второй источник света
	XMMATRIX mRotate = XMMatrixRotationY(-2.0f * t);
	XMVECTOR vLightDir = XMLoadFloat4(&vLightDirs[1]);
	vLightDir = XMVector3Transform(vLightDir, mRotate);
	XMStoreFloat4(&vLightDirs[1], vLightDir);

	// При помощи трансформаций поворачиваем первый источник света
	mRotate = XMMatrixRotationY(0.5f * t);
	vLightDir = XMLoadFloat4(&vLightDirs[0]);
	vLightDir = XMVector3Transform(vLightDir, mRotate);
	XMStoreFloat4(&vLightDirs[0], vLightDir);
}

void UpdateMatrix(UINT nLightIndex)
{
	if (nLightIndex == MX_SETWORLD) {
		// Если рисуем центральный куб: его надо просто вращать
		g_World = g_modelMatrix;
		nLightIndex = 0;
	}
	else if (nLightIndex < 2) {
		// Если рисуем источники света: перемещаем матрицу в точку и уменьшаем в 5 раз
		g_World = XMMatrixTranslationFromVector(10.0f * XMLoadFloat4(&vLightDirs[nLightIndex]));
		XMMATRIX mLightScale = XMMatrixScaling(0.2f, 0.2f, 0.2f);
		g_World = mLightScale * g_World;
	}
	else {
		nLightIndex = 0;
	}

	ConstantBufferMatrixes cb1;
	ConstantBufferLight cb2;
	cb1.mWorld = XMMatrixTranspose(g_World); // загружаем в него матрицы
	cb1.mView = XMMatrixTranspose(g_View);
	cb1.mProjection = XMMatrixTranspose(g_Projection);
	XMVECTOR determinant;
	cb1.mTransInvWorld = XMMatrixTranspose(XMMatrixInverse(&determinant, g_World));
	cb2.vLightDir[0] = vLightDirs[0];          // загружаем данные о свете
	cb2.vLightDir[1] = vLightDirs[1];
	cb2.vLightColor[0] = vLightColors[0];
	cb2.vLightColor[1] = vLightColors[1];
	cb2.vCamPos = XMFLOAT4(camera.Position.x, camera.Position.y, camera.Position.z, 1.f);

	g_pDeviceContext->UpdateSubresource(g_pCBMatrixes, 0, NULL, &cb1, 0, 0);
	g_pDeviceContext->UpdateSubresource(g_pCBLight, 0, NULL, &cb2, 0, 0);
}

void Render()
{
	g_pDeviceContext->OMSetRenderTargets(1, &g_pRenderTargetView, g_pDepthStencilView);
	float Color[4]{ 0.02f, 0.02f, 0.02f, 1.0f };
	g_pDeviceContext->ClearRenderTargetView(g_pRenderTargetView, Color);
	g_pDeviceContext->ClearDepthStencilView(g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.f, 0);

	UpdateLight();

	// 2) Устанавливаем шейдеры и константные буферы
	g_pDeviceContext->VSSetShader(g_pVertexShader, NULL, 0);
	g_pDeviceContext->VSSetConstantBuffers(0, 1, &g_pCBMatrixes);
	g_pDeviceContext->VSSetConstantBuffers(1, 1, &g_pCBLight);
	g_pDeviceContext->PSSetConstantBuffers(0, 1, &g_pCBMatrixes);
	g_pDeviceContext->PSSetConstantBuffers(1, 1, &g_pCBLight);
	g_pDeviceContext->PSSetSamplers(0, 1, &g_pSamplerLinear);

	// Рисуем все источники света
		// 1) Устанавливаем новый пиксельный шейдер
	UINT stride = sizeof(Vertex);
	UINT offset = 0;
	g_pDeviceContext->PSSetShader(g_pPixelShaderSolid, NULL, 0);
	g_pDeviceContext->IASetVertexBuffers(0, 1, &g_pVertexBuffer, &stride, &offset);
	g_pDeviceContext->IASetIndexBuffer(g_pIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
	for (int m = 0; m < 2; m++)
	{
		// 2) Устанавливаем матрицу мира источника света
		UpdateMatrix(m);
		// 3) Рисуем в заднем буфере 36 вершин
		g_pDeviceContext->DrawIndexed(36, 0, 0);
	}

	UpdateMatrix(MX_SETWORLD);
	g_pDeviceContext->PSSetShader(g_pPixelShader, NULL, 0);
	g_pDeviceContext->IASetInputLayout(g_pInputLayout);

	g_pDeviceContext->PSSetShaderResources(4, 1, &g_pIrradianceShaderResourceView);
	g_pDeviceContext->PSSetShaderResources(5, 1, &g_pPreFilterShaderResourceView);
	g_pDeviceContext->PSSetShaderResources(6, 1, &g_pBRDFShaderResourceView);

	model->Draw(g_pDeviceContext, g_pVertexShader, g_pPixelShader);

	render_cubemap(g_pCubemapVertexShader, g_pCubemapPixelShader, g_pCubemapShaderResourceView);

	ImguiRender();

	g_pSwapChain->Present(1, 0);
}

void ImguiInit()
{
	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // IF using Docking Branch   

	// Setup Platform/Renderer backends

	ImGui_ImplGlfw_InitForOther(g_pWindow, true);
	ImGui_ImplDX11_Init(g_pd3dDevice, g_pDeviceContext);
}

void ImguiFrame()
{
	// (Your code process and dispatch Win32 messages)
	// Start the Dear ImGui frame
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	//ImGui::ShowDemoWindow(); // Show demo window! :)
}

void ImguiRender()
{
	// Rendering
	// (Your code clears your framebuffer, renders your other stuff etc.)
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	// (Your code calls swapchain's Present() function)
}

void ImguiCleanup()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

