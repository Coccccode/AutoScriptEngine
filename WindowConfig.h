#pragma once
#include <string>
#include <Windows.h>
enum ControlType
{
	AdbType = 0,
	WindowApiType
};
class WindowConfig
{
public:
	WindowConfig();
	WindowConfig(std::string filepath);
	LPCWSTR windowName;
	bool isSub;
	ControlType controlType;
};

