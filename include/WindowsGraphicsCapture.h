#pragma once

#include <Windows.h>
#include <opencv2/core.hpp>

class WindowsGraphicsCapture
{
public:
	WindowsGraphicsCapture();
	~WindowsGraphicsCapture();

	cv::Mat capture(HWND hwnd);

private:
	struct Impl;
	Impl* impl;
};
