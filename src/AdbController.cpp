#include "AdbController.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <array>

AdbController::AdbController(const std::string& adbPath, const std::string& deviceSerial)
	: m_adbPath(adbPath)
	, m_deviceSerial(deviceSerial)
	, m_width(0)
	, m_height(0)
{
	captureScreen();
}

std::string AdbController::adbShell(const std::string& cmd)
{
	std::string fullCmd = m_adbPath;
	if (!m_deviceSerial.empty())
	{
		fullCmd += " -s " + m_deviceSerial;
	}
	fullCmd += " " + cmd + " 2>&1";

	std::array<char, 4096> buffer;
	std::string result;

	FILE* pipe = _popen(fullCmd.c_str(), "r");
	if (!pipe)
	{
		std::cerr << "failed to execute ADB command: " << fullCmd << std::endl;
		return "";
	}

	while (fgets(buffer.data(), (int)buffer.size(), pipe) != nullptr)
	{
		result += buffer.data();
	}

	int ret = _pclose(pipe);
	if (ret != 0)
	{
		std::cerr << "ADB command failed, code: " << ret << ", output: " << result << std::endl;
	}

	return result;
}

cv::Mat AdbController::captureScreen()
{
	std::string fullCmd = m_adbPath;
	if (!m_deviceSerial.empty())
	{
		fullCmd += " -s " + m_deviceSerial;
	}
	fullCmd += " exec-out screencap -p";

	FILE* pipe = _popen(fullCmd.c_str(), "rb");
	if (!pipe)
	{
		std::cerr << "failed to capture screen through ADB" << std::endl;
		return cv::Mat();
	}

	std::vector<unsigned char> pngData;
	unsigned char buf[8192];
	size_t bytesRead;
	while ((bytesRead = fread(buf, 1, sizeof(buf), pipe)) > 0)
	{
		pngData.insert(pngData.end(), buf, buf + bytesRead);
	}
	_pclose(pipe);

	if (pngData.empty())
	{
		std::cerr << "failed to capture screen through ADB" << std::endl;
		return cv::Mat();
	}

	cv::Mat img = cv::imdecode(pngData, cv::IMREAD_COLOR);
	if (!img.empty())
	{
		m_width = img.cols;
		m_height = img.rows;
	}
	return img;
}

void AdbController::click(int x, int y)
{
	int offsetX = CommonMath::random(-3, 3);
	int offsetY = CommonMath::random(-3, 3);
	int cx = x + offsetX;
	int cy = y + offsetY;

	char cmd[128];
	sprintf_s(cmd, "shell input tap %d %d", cx, cy);
	adbShell(cmd);

	int sleepMs = CommonMath::random(50, 150);
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
}

void AdbController::doubleClick(int x, int y)
{
	click(x, y);
	int sleepMs = CommonMath::random(80, 200);
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
	click(x, y);
}

void AdbController::longClick(int x, int y, int durationMs)
{
	int offsetX = CommonMath::random(-3, 3);
	int offsetY = CommonMath::random(-3, 3);
	int cx = x + offsetX;
	int cy = y + offsetY;
	int duration = durationMs + CommonMath::random(-100, 100);
	if (duration < 100) duration = 100;

	char cmd[256];
	sprintf_s(cmd, "shell input swipe %d %d %d %d %d", cx, cy, cx, cy, duration);
	adbShell(cmd);

	int sleepMs = CommonMath::random(50, 150);
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
}

void AdbController::swipe(int x1, int y1, int x2, int y2, int durationMs)
{
	int duration = durationMs + CommonMath::random(-50, 50);
	if (duration < 50) duration = 50;

	char cmd[256];
	sprintf_s(cmd, "shell input swipe %d %d %d %d %d", x1, y1, x2, y2, duration);
	adbShell(cmd);

	int sleepMs = CommonMath::random(100, 300);
	std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
}

void AdbController::drag(int x1, int y1, int x2, int y2, int durationMs)
{
	swipe(x1, y1, x2, y2, durationMs);
}

int AdbController::screenWidth()
{
	if (m_width == 0) captureScreen();
	return m_width;
}

int AdbController::screenHeight()
{
	if (m_height == 0) captureScreen();
	return m_height;
}
