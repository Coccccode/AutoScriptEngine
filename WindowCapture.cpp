#include "WindowCapture.h"
#include <stdio.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <cassert>
#include <Windows.h>
#include <memory>
WindowCapture::WindowCapture(WindowConfig config)
{
	this->config = config;
	hwnd = ::FindWindowW(NULL, config.windowName);
	if (config.isSub)
	{
		hwnd = ::FindWindowExW(hwnd, NULL, NULL, NULL);
	}
    std::cout << "句柄为" << hwnd;
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    // 创建位图
    width = rcClient.right - rcClient.left;
    height = rcClient.bottom - rcClient.top;
}


cv::Mat WindowCapture::capture()
{
	   // 获取窗口的设备上下文
    HDC hdcWindow = GetDC(hwnd);

    // 获取窗口的尺寸
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    // 创建位图
    width = rcClient.right - rcClient.left;
    height = rcClient.bottom - rcClient.top;
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, width, height);

    // 创建内存设备上下文
    HDC hdcMemDC = CreateCompatibleDC(hdcWindow);

    // 将位图选入内存设备上下文
    SelectObject(hdcMemDC, hBitmap);

    // 将窗口内容拷贝到位图中
    BitBlt(hdcMemDC, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY);
    cv::Mat mat(height, width, CV_8UC4);
    BITMAPINFOHEADER bi = { sizeof(bi), width, -height, 1, 32, BI_RGB };
    GetDIBits(hdcMemDC, hBitmap, 0, height, mat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // 释放资源
    DeleteObject(hBitmap);
    DeleteDC(hdcMemDC);
    ReleaseDC(hwnd, hdcWindow);
    return mat;
}

bool WindowCapture::saveImg(cv::Mat saveImg)
{
    cv::imwrite("save.jpg", saveImg);
    return true;
}
