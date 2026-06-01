#pragma once
#include "Controller.h"
#include <stdio.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <cassert>
#include <Windows.h>
#include "CommonMath.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

class MouseController :
    public Controller
{
public:
	MouseController() = default;

	cv::Mat captureScreen() override { return cv::Mat(); }
	int screenWidth() override { return 0; }
	int screenHeight() override { return 0; }

	void click(int x, int y) override;
	void doubleClick(int x, int y) override;
	void longClick(int x, int y, int durationMs = 1000) override;
	void swipe(int x1, int y1, int x2, int y2, int durationMs = 300) override;
	void drag(int x1, int y1, int x2, int y2, int durationMs = 500) override;

	void click_point_random(cv::Point point, HWND& hWnd);
	void click_point_random(unsigned long x, unsigned long y, HWND& hWnd);
	void click_point(unsigned long x, unsigned long y, HWND& hWnd);
	void click_area_random(cv::Point a, cv::Point b, HWND& hWnd);
	void long_click_point(unsigned long x, unsigned long y, HWND& hWnd);
	void double_click_point(unsigned long x, unsigned long y, HWND& hWnd);
	void swipe_points(unsigned long x1, unsigned long y1, unsigned long x2, unsigned long y2, HWND& hWnd, int durationMs = 300);
	void drag_points(unsigned long x1, unsigned long y1, unsigned long x2, unsigned long y2, HWND& hWnd, int durationMs = 500);
};
