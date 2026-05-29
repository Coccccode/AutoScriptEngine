#pragma once
#include <string>
#include <Windows.h>
#include <json/json.h>
#include <iostream>
#include <fstream>
#include <string>

enum ControlType
{
	AdbType = 0,
	WindowApiType = 1
};

class WindowConfig
{
public:
	WindowConfig();
	WindowConfig(std::string filepath);

	Json::Value parsezJsonFromString(const std::string& jsonString);
	Json::Value readTaskJson(std::string filePath);

	LPCWSTR windowName;
	LPCWSTR targetWindowName;
	std::wstring windowNameStorage;
	std::wstring targetWindowNameStorage;
	std::string adbPath;
	std::string deviceSerial;
	std::string captureBackend;
	int screenshotIntervalMs;
	HWND configuredHwnd;
	HWND configuredTargetHwnd;

	enum WindowType
	{
		LEIDIAN = 1,
		MUMU    = 2
	};

	int windowType;
	ControlType controlType;
};
