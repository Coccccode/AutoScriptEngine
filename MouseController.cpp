#include "MouseController.h"
#include <chrono>   // std::chrono::seconds
#include <thread>   // std::this_thread::sleep_for
void MouseController::click_point_random(unsigned long x, unsigned long y, HWND& hWnd)
{
	x = x + CommonMath::random(-10, 10);
	y = y + CommonMath::random(-10, 10);
	SendMessage(hWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
	SendMessage(hWnd, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(x, y));
};
void MouseController::click_point(unsigned long x, unsigned long y, HWND& hWnd)
{
	int sleepTime = CommonMath::random(200, 500);
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
	SendMessage(hWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
	SendMessage(hWnd, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(x, y));
}
void MouseController::click_area_random(cv::Point a, cv::Point b, HWND& hWnd)
{
	int x = CommonMath::random(a.x, b.x);
	int y = CommonMath::random(a.y, b.y);
	click_point(x, y, hWnd);
}
;