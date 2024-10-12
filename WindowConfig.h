#pragma once
#include <string>
#include <Windows.h>
#include <json/json.h>
#include <iostream>
#include <fstream>
enum ControlType
{
	AdbType = 0,
	WindowApiType
};
class WindowConfig
{
public:
	WindowConfig();
	Json::Value parseJsonFromString(const std::string& jsonString);
	Json::Value readTaskJson(std::string filePath);
	WindowConfig(std::string filepath);
	LPCSTR windowName;
	bool isSub;
	ControlType controlType;
};

