#include "WindowController.h"
#include <thread>
#include <chrono>
#include <iostream>

WindowController::WindowController(WindowConfig* config)
	: m_hwnd(NULL)
	, m_capture(NULL)
{
	m_capture = new WindowCapture(config);
	m_hwnd = m_capture->getHwnd();
}

WindowController::~WindowController()
{
	delete m_capture;
}

cv::Mat WindowController::captureScreen()
{
	return m_capture->capture();
}

HWND WindowController::getHwnd()
{
	return m_hwnd;
}

cv::Point WindowController::screenToClient(const cv::Point& screenPt)
{
	POINT pt = { screenPt.x, screenPt.y };
	ScreenToClient(m_hwnd, &pt);
	return cv::Point(pt.x, pt.y);
}

cv::Point WindowController::clientToScreenPoint(int x, int y)
{
	POINT pt = { x, y };
	ClientToScreen(m_hwnd, &pt);
	return cv::Point(pt.x, pt.y);
}

void WindowController::sendMouseDown(int x, int y)
{
	SendMessage(m_hwnd, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
}

void WindowController::sendMouseUp(int x, int y)
{
	SendMessage(m_hwnd, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
}

void WindowController::sendMouseMove(int x, int y)
{
	SendMessage(m_hwnd, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(x, y));
}

void WindowController::click(int x, int y)
{
	int cx = x + CommonMath::random(-5, 5);
	int cy = y + CommonMath::random(-5, 5);
	int downTime = CommonMath::random(30, 80);

	sendMouseDown(cx, cy);
	std::this_thread::sleep_for(std::chrono::milliseconds(downTime));
	sendMouseUp(cx, cy);

	int sleepMs = CommonMath::random(50, 150);
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
}

void WindowController::doubleClick(int x, int y)
{
	click(x, y);
	int interval = CommonMath::random(80, 200);
	std::this_thread::sleep_for(std::chrono::milliseconds(interval));
	click(x, y);
}

void WindowController::longClick(int x, int y, int durationMs)
{
	int cx = x + CommonMath::random(-5, 5);
	int cy = y + CommonMath::random(-5, 5);
	int duration = durationMs + CommonMath::random(-100, 100);
	if (duration < 100) duration = 100;

	sendMouseDown(cx, cy);
	std::this_thread::sleep_for(std::chrono::milliseconds(duration));
	sendMouseUp(cx, cy);

	int sleepMs = CommonMath::random(50, 150);
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
}

void WindowController::swipe(int x1, int y1, int x2, int y2, int durationMs)
{
	int steps = 24;
	int stepDelay = durationMs / steps;
	if (stepDelay < 8) stepDelay = 8;

	double dx = (double)(x2 - x1) / steps;
	double dy = (double)(y2 - y1) / steps;

	HWND target = m_hwnd;
	SendMessage(target, WM_MOUSEACTIVATE, (WPARAM)GetAncestor(target, GA_ROOT), MAKELPARAM(HTCLIENT, WM_LBUTTONDOWN));
	SendMessage(target, WM_SETCURSOR, (WPARAM)target, MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
	SendMessage(target, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x1, y1));
	std::this_thread::sleep_for(std::chrono::milliseconds(stepDelay));

	for (int i = 1; i <= steps; i++)
	{
		int cx = (int)(x1 + dx * i);
		int cy = (int)(y1 + dy * i);
		SendMessage(target, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(cx, cy));
		std::this_thread::sleep_for(std::chrono::milliseconds(stepDelay));
	}

	SendMessage(target, WM_LBUTTONUP, 0, MAKELPARAM(x2, y2));

	int sleepMs = CommonMath::random(50, 150);
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
}
void WindowController::drag(int x1, int y1, int x2, int y2, int durationMs)
{
	swipe(x1, y1, x2, y2, durationMs);
}

int WindowController::screenWidth()
{
	cv::Mat img = captureScreen();
	return img.empty() ? 0 : img.cols;
}

int WindowController::screenHeight()
{
	cv::Mat img = captureScreen();
	return img.empty() ? 0 : img.rows;
}
