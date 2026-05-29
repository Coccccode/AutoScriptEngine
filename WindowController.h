#pragma once
#include "Controller.h"
#include "WindowCapture.h"
#include "WindowConfig.h"
#include "CommonMath.h"
#include <Windows.h>

class WindowController : public Controller
{
public:
	explicit WindowController(WindowConfig* config);
	~WindowController();

	cv::Mat captureScreen() override;
	void click(int x, int y) override;
	void doubleClick(int x, int y) override;
	void longClick(int x, int y, int durationMs = 1000) override;
	void swipe(int x1, int y1, int x2, int y2, int durationMs = 300) override;
	void drag(int x1, int y1, int x2, int y2, int durationMs = 500) override;
	int screenWidth() override;
	int screenHeight() override;

	HWND getHwnd();
	cv::Point screenToClient(const cv::Point& screenPt);

private:
	WindowCapture* m_capture;
	HWND m_hwnd;

	cv::Point clientToScreenPoint(int x, int y);
	void sendMouseDown(int x, int y);
	void sendMouseUp(int x, int y);
	void sendMouseMove(int x, int y);
};
