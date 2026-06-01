#include "MouseController.h"
#include <chrono>
#include <thread>

void MouseController::click(int x, int y)
{
	std::cerr << "MouseController::click requires HWND; use click_point" << std::endl;
}

void MouseController::doubleClick(int x, int y)
{
	std::cerr << "MouseController::doubleClick requires HWND; use double_click_point" << std::endl;
}

void MouseController::longClick(int x, int y, int durationMs)
{
	std::cerr << "MouseController::longClick requires HWND; use long_click_point" << std::endl;
}

void MouseController::swipe(int x1, int y1, int x2, int y2, int durationMs)
{
	std::cerr << "MouseController::swipe requires HWND; use swipe_points" << std::endl;
}

void MouseController::drag(int x1, int y1, int x2, int y2, int durationMs)
{
	std::cerr << "MouseController::drag requires HWND; use drag_points" << std::endl;
}

void MouseController::click_point_random(cv::Point point, HWND& hWnd)
{
	unsigned long x = point.x + CommonMath::random(-10, 10);
	unsigned long y = point.y + CommonMath::random(-10, 10);
	int sleepTime = CommonMath::random(100, 200);
	SendMessage(hWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
	SendMessage(hWnd, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(x, y));
}

void MouseController::click_point_random(unsigned long x, unsigned long y, HWND& hWnd)
{
	x = x + CommonMath::random(-10, 10);
	y = y + CommonMath::random(-10, 10);
	int sleepTime = CommonMath::random(100, 200);
	SendMessage(hWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
	SendMessage(hWnd, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(x, y));
}

void MouseController::click_point(unsigned long x, unsigned long y, HWND& hWnd)
{
	int sleepTime = CommonMath::random(50, 120);
	SendMessage(hWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
	SendMessage(hWnd, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(x, y));
}

void MouseController::long_click_point(unsigned long x, unsigned long y, HWND& hWnd)
{
	int sleepTime = CommonMath::random(1000, 1500);
	SendMessage(hWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
	SendMessage(hWnd, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(x, y));
}

void MouseController::double_click_point(unsigned long x, unsigned long y, HWND& hWnd)
{
	click_point(x, y, hWnd);
	int interval = CommonMath::random(80, 200);
	std::this_thread::sleep_for(std::chrono::milliseconds(interval));
	click_point(x, y, hWnd);
}

void MouseController::click_area_random(cv::Point a, cv::Point b, HWND& hWnd)
{
	int x = CommonMath::random(a.x, b.x);
	int y = CommonMath::random(a.y, b.y);
	click_point(x, y, hWnd);
}

void MouseController::swipe_points(unsigned long x1, unsigned long y1, unsigned long x2, unsigned long y2, HWND& hWnd, int durationMs)
{
	int steps = 20;
	int stepDelay = durationMs / steps;
	if (stepDelay < 5) stepDelay = 5;

	double dx = (double)((long)x2 - (long)x1) / steps;
	double dy = (double)((long)y2 - (long)y1) / steps;

	SendMessage(hWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x1, y1));
	std::this_thread::sleep_for(std::chrono::milliseconds(stepDelay));

	for (int i = 1; i <= steps; i++)
	{
		int cx = (int)(x1 + dx * i);
		int cy = (int)(y1 + dy * i);
		SendMessage(hWnd, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(cx, cy));
		std::this_thread::sleep_for(std::chrono::milliseconds(stepDelay));
	}

	SendMessage(hWnd, WM_LBUTTONUP, 0, MAKELPARAM(x2, y2));

	int sleepMs = CommonMath::random(50, 150);
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
}

void MouseController::drag_points(unsigned long x1, unsigned long y1, unsigned long x2, unsigned long y2, HWND& hWnd, int durationMs)
{
	swipe_points(x1, y1, x2, y2, hWnd, durationMs);
}
