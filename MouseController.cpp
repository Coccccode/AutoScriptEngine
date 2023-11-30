#include "MouseController.h"
void MouseController::click_point_random(unsigned long x, unsigned long y, HWND& hWnd)
{
	x = x + CommonMath::random(-10, 10);
	y = y + CommonMath::random(-10, 10);
	SendMessage(hWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
	SendMessage(hWnd, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(x, y));
};
void MouseController::click_point(unsigned long x, unsigned long y, HWND& hWnd)
{
	SendMessage(hWnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
	SendMessage(hWnd, WM_LBUTTONUP, MK_LBUTTON, MAKELPARAM(x, y));
};