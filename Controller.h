#pragma once
#include <Windows.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

class Controller
{
public:
	virtual ~Controller() = default;

	virtual cv::Mat captureScreen() = 0;
	virtual void click(int x, int y) = 0;
	virtual void doubleClick(int x, int y) = 0;
	virtual void longClick(int x, int y, int durationMs = 1000) = 0;
	virtual void swipe(int x1, int y1, int x2, int y2, int durationMs = 300) = 0;
	virtual void drag(int x1, int y1, int x2, int y2, int durationMs = 500) = 0;
	virtual int screenWidth() = 0;
	virtual int screenHeight() = 0;
};
