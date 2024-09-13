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
	if (hwnd != NULL && config.isSub)
	{
		hwnd = ::FindWindowExW(hwnd, NULL, NULL, NULL);
	}
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
    int dpi = GetDpiForWindow(hwnd);
    double scale = dpi / 96.0;
    // 获取窗口的尺寸
    RECT rcClient;
    GetClientRect(hwnd, &rcClient);

    // 创建位图
    width = (rcClient.right - rcClient.left) * scale;
    height = (rcClient.bottom - rcClient.top) * scale;
    HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, width, height);
    // 创建内存设备上下文
    HDC hdcMemDC = CreateCompatibleDC(hdcWindow);

    // 将位图选入内存设备上下文
    SelectObject(hdcMemDC, hBitmap);
    printf("%d", dpi);
    // 将窗口内容拷贝到位图中
    BitBlt(hdcMemDC, 0, 0, width, height, hdcWindow, 0, 0, SRCCOPY);
    cv::Mat mat(height, width, CV_8UC3);
    BITMAPINFOHEADER bi = { sizeof(bi), width, -height, 1, 24, BI_RGB };
    GetDIBits(hdcMemDC, hBitmap, 0, height, mat.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    // 释放资源
    DeleteObject(hBitmap);
    DeleteDC(hdcMemDC);
    ReleaseDC(hwnd, hdcWindow);
    return mat;
}

bool WindowCapture::saveImg(cv::Mat saveImg)
{
    static int id = 0;
    char s[100];
    sprintf_s(s, "save%d.jpg", id);
    cv::imwrite(s, saveImg);
    id++;
    return true;
}

HWND WindowCapture::getHwnd()
{
    return hwnd;
}

