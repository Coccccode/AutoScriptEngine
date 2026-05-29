#include "WindowCapture.h"
#include "WindowsGraphicsCapture.h"
#include <iostream>
#include <Windows.h>

namespace
{
	bool matchTextOrClass(HWND hwnd, LPCWSTR value)
	{
		if (value == NULL) return false;

		wchar_t buf[256] = { 0 };
		GetWindowTextW(hwnd, buf, 256);
		if (wcscmp(buf, value) == 0) return true;

		GetClassNameW(hwnd, buf, 256);
		return _wcsicmp(buf, value) == 0;
	}

	HWND findDescendant(HWND parent, LPCWSTR target)
	{
		HWND child = FindWindowExW(parent, NULL, NULL, NULL);
		while (child != NULL)
		{
			HWND nested = findDescendant(child, target);
			if (nested != NULL) return nested;

			if (matchTextOrClass(child, target)) return child;

			child = FindWindowExW(parent, child, NULL, NULL);
		}
		return NULL;
	}

	struct TopWindowSearch
	{
		LPCWSTR title;
		LPCWSTR className;
		HWND hwnd;
	};

	BOOL CALLBACK enumTopWindowProc(HWND candidate, LPARAM lParam)
	{
		TopWindowSearch* search = (TopWindowSearch*)lParam;
		if (!IsWindowVisible(candidate)) return TRUE;

		wchar_t title[256] = { 0 };
		wchar_t className[256] = { 0 };
		GetWindowTextW(candidate, title, 256);
		GetClassNameW(candidate, className, 256);

		if (wcscmp(title, search->title) == 0 &&
			(search->className == NULL || _wcsicmp(className, search->className) == 0))
		{
			search->hwnd = candidate;
			return FALSE;
		}
		return TRUE;
	}

	HWND findTopWindow(LPCWSTR title, LPCWSTR className)
	{
		TopWindowSearch search = { title, className, NULL };
		EnumWindows(enumTopWindowProc, (LPARAM)&search);
		return search.hwnd;
	}
}

WindowCapture::WindowCapture(WindowConfig *config)
{
	this->config = config;
	if (config->configuredHwnd != NULL && IsWindow(config->configuredHwnd))
	{
		hwnd = config->configuredHwnd;
	}
	if (config->windowType == WindowConfig::MUMU)
	{
		if (hwnd == NULL)
		{
			hwnd = findTopWindow(config->windowName, L"Qt5156QWindowIcon");
		}
		if (hwnd == NULL)
		{
			hwnd = findTopWindow(L"MuMu\u6a21\u62df\u5668", NULL);
		}
	}
	if (hwnd == NULL)
	{
		hwnd = ::FindWindowW(NULL, config->windowName);
	}

	if (hwnd == NULL) return;

	captureHwnd = hwnd;

	if (config->configuredTargetHwnd != NULL && IsWindow(config->configuredTargetHwnd))
	{
		captureHwnd = config->configuredTargetHwnd;
	}
	else if (config->targetWindowName != NULL)
	{
		HWND target = findDescendant(hwnd, config->targetWindowName);
		if (target != NULL)
		{
			captureHwnd = target;
			if (config->windowType != WindowConfig::MUMU)
			{
				hwnd = target;
			}
		}
	}

	if (hwnd != NULL)
	{
		RECT rcClient;
		GetClientRect(hwnd, &rcClient);
		width = rcClient.right - rcClient.left;
		height = rcClient.bottom - rcClient.top;
	}

	if (config->windowType == WindowConfig::MUMU && config->captureBackend == "wgc")
	{
		wgcCapture.reset(new WindowsGraphicsCapture());
	}
}

WindowCapture::~WindowCapture() = default;

cv::Mat WindowCapture::capture()
{
	if (hwnd == NULL || !IsWindow(hwnd)) return cv::Mat();

	RECT rc;
	if (!GetClientRect(hwnd, &rc) || rc.right <= 0 || rc.bottom <= 0) return cv::Mat();

	int dpi = GetDpiForWindow(hwnd);
	double scale = dpi / 96.0;
	int w = (int)((rc.right - rc.left) * scale);
	int h = (int)((rc.bottom - rc.top) * scale);

	if (w <= 0 || h <= 0) return cv::Mat();

	auto captureVisibleClient = [&]() -> cv::Mat {
		POINT pt = { 0, 0 };
		ClientToScreen(hwnd, &pt);

		HDC hdcScreen = GetDC(NULL);
		HDC hdcMem = CreateCompatibleDC(hdcScreen);
		HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, w, h);
		HGDIOBJ hOld = SelectObject(hdcMem, hBitmap);

		BitBlt(hdcMem, 0, 0, w, h, hdcScreen, pt.x, pt.y, SRCCOPY | CAPTUREBLT);

		cv::Mat mat(h, w, CV_8UC3);
		BITMAPINFOHEADER bi = { sizeof(bi), w, -h, 1, 24, BI_RGB };
		GetDIBits(hdcMem, hBitmap, 0, h, mat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

		SelectObject(hdcMem, hOld);
		DeleteObject(hBitmap);
		DeleteDC(hdcMem);
		ReleaseDC(NULL, hdcScreen);

		return mat;
	};

	auto captureVisibleWindow = [&]() -> cv::Mat {
		RECT windowRect;
		if (!GetWindowRect(hwnd, &windowRect)) return cv::Mat();

		int windowW = windowRect.right - windowRect.left;
		int windowH = windowRect.bottom - windowRect.top;
		if (windowW <= 0 || windowH <= 0) return cv::Mat();

		HDC hdcScreen = GetDC(NULL);
		HDC hdcMem = CreateCompatibleDC(hdcScreen);
		HBITMAP hBitmap = CreateCompatibleBitmap(hdcScreen, windowW, windowH);
		HGDIOBJ hOld = SelectObject(hdcMem, hBitmap);

		BitBlt(hdcMem, 0, 0, windowW, windowH, hdcScreen, windowRect.left, windowRect.top, SRCCOPY | CAPTUREBLT);

		cv::Mat mat(windowH, windowW, CV_8UC3);
		BITMAPINFOHEADER bi = { sizeof(bi), windowW, -windowH, 1, 24, BI_RGB };
		GetDIBits(hdcMem, hBitmap, 0, windowH, mat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

		SelectObject(hdcMem, hOld);
		DeleteObject(hBitmap);
		DeleteDC(hdcMem);
		ReleaseDC(NULL, hdcScreen);

		return mat;
	};

	auto cropToCaptureWindow = [&](const cv::Mat& image) -> cv::Mat {
		if (image.empty() || captureHwnd == NULL || captureHwnd == hwnd) return image;

		RECT windowRect;
		RECT captureRect;
		if (!GetWindowRect(hwnd, &windowRect) || !GetWindowRect(captureHwnd, &captureRect))
		{
			return image;
		}

		int windowW = windowRect.right - windowRect.left;
		int windowH = windowRect.bottom - windowRect.top;
		if (windowW <= 0 || windowH <= 0) return image;

		double scaleX = (double)image.cols / windowW;
		double scaleY = (double)image.rows / windowH;

		int x = (int)((captureRect.left - windowRect.left) * scaleX);
		int y = (int)((captureRect.top - windowRect.top) * scaleY);
		int cropW = (int)((captureRect.right - captureRect.left) * scaleX);
		int cropH = (int)((captureRect.bottom - captureRect.top) * scaleY);

		cv::Rect roi(x, y, cropW, cropH);
		cv::Rect bounds(0, 0, image.cols, image.rows);
		roi = roi & bounds;
		if (roi.width <= 0 || roi.height <= 0) return image;

		return image(roi).clone();
	};

	if (config->windowType == WindowConfig::MUMU)
	{
		cv::Mat mat;
		bool capturedTopWindow = false;
		if (config->captureBackend == "wgc" && wgcCapture)
		{
			mat = wgcCapture->capture(hwnd);
			capturedTopWindow = !mat.empty();
			if (mat.empty() && captureHwnd != hwnd)
			{
				mat = wgcCapture->capture(captureHwnd);
			}
			/*
			if (mat.empty() && captureHwnd != hwnd)
			{
				mat = wgcCapture->capture(hwnd);
				capturedTopWindow = !mat.empty();
			}
			*/
		}
		if (mat.empty())
		{
			mat = captureVisibleWindow();
			capturedTopWindow = !mat.empty();
		}
		if (capturedTopWindow)
		{
			mat = cropToCaptureWindow(mat);
		}
		saveImg(mat);
		return mat;
	}

	HDC hdcWindow = GetDC(hwnd);
	HDC hdcMem = CreateCompatibleDC(hdcWindow);
	HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, w, h);
	HGDIOBJ hOld = SelectObject(hdcMem, hBitmap);

	BOOL success = PrintWindow(hwnd, hdcMem, PW_RENDERFULLCONTENT);

	if (!success)
	{
		SelectObject(hdcMem, hOld);
		DeleteObject(hBitmap);
		DeleteDC(hdcMem);
		ReleaseDC(hwnd, hdcWindow);

		cv::Mat mat = captureVisibleClient();
		saveImg(mat);
		return mat;
	}

	cv::Mat mat(h, w, CV_8UC3);
	BITMAPINFOHEADER bi = { sizeof(bi), w, -h, 1, 24, BI_RGB };
	GetDIBits(hdcMem, hBitmap, 0, h, mat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

	SelectObject(hdcMem, hOld);
	DeleteObject(hBitmap);
	DeleteDC(hdcMem);
	ReleaseDC(hwnd, hdcWindow);
	saveImg(mat);
	return mat;
}
bool WindowCapture::saveImg(cv::Mat saveImg)
{
	if (saveImg.empty()) return false;

	static int id = 0;
	///char s[100];
	//sprintf_s(s, "save%d.png", id);
	//cv::imwrite(s, saveImg);
	id++;
	return true;
}

HWND WindowCapture::getHwnd()
{
	if (captureHwnd != NULL) return captureHwnd;
	return hwnd;
}
