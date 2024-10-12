#pragma once
#include <string>
#include <json/json.h>
#include <fstream>
#include <queue>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <windows.h>
#include "WindowCapture.h"
#include "WindowConfig.h"
#include <json/json.h>
#include "OpencvAPI.h"
#include "MouseController.h"
class TaskRunner
{
public:
	TaskRunner();
	std::queue<std::string> taskList;
	void start();
	void popTask(std::string taskName);
	void runTask(std::string taskName);
	Json::Value readTaskJson(std::string filePath);
	Json::Value parseJsonFromString(const std::string& jsonString);
	~TaskRunner();
	OpencvAPI* cvapi;
	WindowCapture* capture;
	WindowConfig *config;
	MouseController* mouse;
};

