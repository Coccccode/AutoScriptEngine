#define _SILENCE_EXPERIMENTAL_COROUTINE_DEPRECATION_WARNINGS

#include "WindowsGraphicsCapture.h"

#include <chrono>
#include <mutex>
#include <thread>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <inspectable.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/base.h>
#include <opencv2/imgproc.hpp>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowsapp.lib")

namespace capture = winrt::Windows::Graphics::Capture;
namespace directx = winrt::Windows::Graphics::DirectX;
namespace d3d11 = winrt::Windows::Graphics::DirectX::Direct3D11;

extern "C" HRESULT __stdcall CreateDirect3D11DeviceFromDXGIDevice(
	IDXGIDevice* dxgiDevice,
	IInspectable** graphicsDevice);

namespace
{
	// 注意：这里删除了原有的 std::mutex g_captureMutex; 

	capture::GraphicsCaptureItem createCaptureItemForWindow(HWND hwnd)
	{
		auto factory = winrt::get_activation_factory<capture::GraphicsCaptureItem>();
		auto interop = factory.as<IGraphicsCaptureItemInterop>();

		capture::GraphicsCaptureItem item = nullptr;
		winrt::check_hresult(interop->CreateForWindow(
			hwnd,
			winrt::guid_of<capture::GraphicsCaptureItem>(),
			winrt::put_abi(item)));

		return item;
	}
}

struct WindowsGraphicsCapture::Impl
{
	std::mutex instanceMutex; // 新增：每个捕捉实例私有的锁
	
	winrt::com_ptr<ID3D11Device> d3dDevice;
	winrt::com_ptr<ID3D11DeviceContext> d3dContext;
	d3d11::IDirect3DDevice winrtDevice = nullptr;

	Impl()
	{
		try
		{
			winrt::init_apartment(winrt::apartment_type::multi_threaded);
		}
		catch (winrt::hresult_error const& error)
		{
			if (error.code() != RPC_E_CHANGED_MODE)
			{
				throw;
			}
		}

		UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

		D3D_FEATURE_LEVEL featureLevels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0
		};
		D3D_FEATURE_LEVEL selectedLevel;

		winrt::check_hresult(D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			flags,
			featureLevels,
			_countof(featureLevels),
			D3D11_SDK_VERSION,
			d3dDevice.put(),
			&selectedLevel,
			d3dContext.put()));

		auto dxgiDevice = d3dDevice.as<IDXGIDevice>();
		winrt::com_ptr<IInspectable> inspectable;
		winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), inspectable.put()));
		winrtDevice = inspectable.as<d3d11::IDirect3DDevice>();
	}

	cv::Mat capture(HWND hwnd)
	{
		capture::GraphicsCaptureItem item = createCaptureItemForWindow(hwnd);
		auto size = item.Size();
		if (size.Width <= 0 || size.Height <= 0) return cv::Mat();

		auto framePool = capture::Direct3D11CaptureFramePool::CreateFreeThreaded(
			winrtDevice,
			directx::DirectXPixelFormat::B8G8R8A8UIntNormalized,
			2,
			size);
		auto session = framePool.CreateCaptureSession(item);
		session.IsCursorCaptureEnabled(false);
		session.StartCapture();

		capture::Direct3D11CaptureFrame frame = nullptr;
		for (int i = 0; i < 20; ++i)
		{
			frame = framePool.TryGetNextFrame();
			if (frame != nullptr) break;
			std::this_thread::sleep_for(std::chrono::milliseconds(20));
		}

		if (frame == nullptr)
		{
			session.Close();
			framePool.Close();
			return cv::Mat();
		}

		auto surface = frame.Surface();
		auto access = surface.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();

		winrt::com_ptr<ID3D11Texture2D> frameTexture;
		winrt::check_hresult(access->GetInterface(winrt::guid_of<ID3D11Texture2D>(), frameTexture.put_void()));

		D3D11_TEXTURE2D_DESC desc;
		frameTexture->GetDesc(&desc);
		if (desc.Width == 0 || desc.Height == 0) return cv::Mat();

		D3D11_TEXTURE2D_DESC stagingDesc = desc;
		stagingDesc.BindFlags = 0;
		stagingDesc.MiscFlags = 0;
		stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		stagingDesc.Usage = D3D11_USAGE_STAGING;

		winrt::com_ptr<ID3D11Texture2D> stagingTexture;
		winrt::check_hresult(d3dDevice->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.put()));
		d3dContext->CopyResource(stagingTexture.get(), frameTexture.get());

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		winrt::check_hresult(d3dContext->Map(stagingTexture.get(), 0, D3D11_MAP_READ, 0, &mapped));

		cv::Mat bgra((int)desc.Height, (int)desc.Width, CV_8UC4);
		for (UINT y = 0; y < desc.Height; ++y)
		{
			memcpy(
				bgra.ptr((int)y),
				(const unsigned char*)mapped.pData + y * mapped.RowPitch,
				desc.Width * 4);
		}

		d3dContext->Unmap(stagingTexture.get(), 0);

		cv::Mat bgr;
		cv::cvtColor(bgra, bgr, cv::COLOR_BGRA2BGR);

		session.Close();
		framePool.Close();
		return bgr;
	}
};

WindowsGraphicsCapture::WindowsGraphicsCapture()
	: impl(nullptr)
{
	try
	{
		impl = new Impl();
	}
	catch (...)
	{
		impl = nullptr;
	}
}

WindowsGraphicsCapture::~WindowsGraphicsCapture()
{
	delete impl;
}

cv::Mat WindowsGraphicsCapture::capture(HWND hwnd)
{
	if (impl == nullptr || hwnd == NULL || !IsWindow(hwnd)) return cv::Mat();

	try
	{
		// 使用实例内部的锁，互不干扰
		std::lock_guard<std::mutex> lock(impl->instanceMutex);
		return impl->capture(hwnd);
	}
	catch (...)
	{
		return cv::Mat();
	}
}