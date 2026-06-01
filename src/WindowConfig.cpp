#include "WindowConfig.h"

namespace
{
	std::wstring utf8ToWide(const std::string& text)
	{
		if (text.empty()) return L"";
		int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
		if (len <= 0) return L"";
		std::wstring wide(len - 1, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wide[0], len);
		return wide;
	}
}

WindowConfig::WindowConfig()
{
	this->windowNameStorage = L"\u96f7\u7535\u6a21\u62df\u5668";
	this->targetWindowNameStorage = L"";
	this->windowName = this->windowNameStorage.c_str();
	this->targetWindowName = NULL;
	this->controlType = WindowApiType;
	this->windowType = LEIDIAN;
	this->adbPath = "adb";
	this->deviceSerial = "";
	this->captureBackend = "wgc";
	this->screenshotIntervalMs = 1000;
	this->configuredHwnd = NULL;
	this->configuredTargetHwnd = NULL;
}

Json::Value WindowConfig::parseJsonFromString(const std::string& jsonString) {
	Json::Reader reader;
	Json::Value root;

	if (!reader.parse(jsonString, root)) {
		std::cerr << "JSON parsing error: " << reader.getFormattedErrorMessages() << std::endl;
	}

	return root;
}

Json::Value WindowConfig::readTaskJson(std::string filePath)
{
	std::ifstream file(filePath);
	if (!file.is_open()) {
		std::cerr << "Failed to open the file." << std::endl;
		return Json::Value();
	}

	std::string jsonData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	Json::Value root = parseJsonFromString(jsonData);

	if (root.isNull()) {
		std::cerr << "JSON parsing error." << std::endl;
		return Json::Value();
	}
	return root;
}

WindowConfig::WindowConfig(std::string filepath)
{
	Json::Value config = readTaskJson(filepath);
	this->windowType = config.get("windowType", 1).asInt();
	this->controlType = (ControlType)config.get("controlType", (int)WindowApiType).asInt();
	this->adbPath = config.get("adbPath", "adb").asString();
	this->deviceSerial = config.get("deviceSerial", "").asString();
	this->captureBackend = config.get("captureBackend", this->controlType == AdbType ? "adb" : "wgc").asString();
	this->screenshotIntervalMs = config.get("screenshotIntervalMs", 1000).asInt();
	this->configuredHwnd = NULL;
	this->configuredTargetHwnd = NULL;

	if (config.isMember("device") && config["device"].isObject())
	{
		const Json::Value& device = config["device"];
		if (device.isMember("hwnd"))
		{
			this->configuredHwnd = (HWND)(uintptr_t)device["hwnd"].asUInt64();
		}
		if (device.isMember("target_hwnd"))
		{
			this->configuredTargetHwnd = (HWND)(uintptr_t)device["target_hwnd"].asUInt64();
		}
	}

	switch (this->windowType)
	{
		case LEIDIAN:
			this->windowNameStorage = L"\u96f7\u7535\u6a21\u62df\u5668";
			this->targetWindowNameStorage = L"";
			break;
		case MUMU:
			this->windowNameStorage = L"MuMu\u5b89\u5353\u8bbe\u5907";
			this->targetWindowNameStorage = L"MuMuNxDevice";
			break;
		default:
			this->windowNameStorage = L"";
			this->targetWindowNameStorage = L"";
			break;
	}

	if (config.isMember("windowName"))
	{
		this->windowNameStorage = utf8ToWide(config["windowName"].asString());
	}
	if (config.isMember("targetWindowName"))
	{
		this->targetWindowNameStorage = utf8ToWide(config["targetWindowName"].asString());
	}

	this->windowName = this->windowNameStorage.empty() ? NULL : this->windowNameStorage.c_str();
	this->targetWindowName = this->targetWindowNameStorage.empty() ? NULL : this->targetWindowNameStorage.c_str();
}
