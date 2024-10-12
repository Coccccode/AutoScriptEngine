#pragma once
#include <string>
#include "WindowConfig.h"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
class WindowCapture
{
private:
	WindowConfig* config;
	HWND hwnd = 0;
	int width;
	int height;
public:
	WindowCapture(WindowConfig *config);
	cv::Mat capture();
	bool saveImg(cv::Mat saveImg);
	HWND getHwnd();
};

