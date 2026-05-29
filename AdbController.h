#pragma once
#include "Controller.h"
#include "CommonMath.h"
#include <string>
#include <cstdio>

class AdbController : public Controller
{
public:
	AdbController(const std::string& adbPath = "adb", const std::string& deviceSerial = "");

	cv::Mat captureScreen() override;
	void click(int x, int y) override;
	void doubleClick(int x, int y) override;
	void longClick(int x, int y, int durationMs = 1000) override;
	void swipe(int x1, int y1, int x2, int y2, int durationMs = 300) override;
	void drag(int x1, int y1, int x2, int y2, int durationMs = 500) override;
	int screenWidth() override;
	int screenHeight() override;

private:
	std::string m_adbPath;
	std::string m_deviceSerial;
	int m_width;
	int m_height;

	std::string adbShell(const std::string& cmd);
};
