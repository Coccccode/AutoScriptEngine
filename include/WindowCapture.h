#pragma once
#include <string>
#include <memory>
#include "WindowConfig.h"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

class WindowsGraphicsCapture;

class WindowCapture
{
private:
	WindowConfig* config;
	HWND hwnd = 0;
	HWND captureHwnd = 0;
	int width;
	int height;
	std::unique_ptr<WindowsGraphicsCapture> wgcCapture;

public:
	WindowCapture(WindowConfig *config);
	~WindowCapture();
	cv::Mat capture();
	bool saveImg(cv::Mat saveImg);
	HWND getHwnd();
};
