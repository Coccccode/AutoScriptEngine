#pragma once
#include <string>
#include <json/json.h>
#include <fstream>
#include <queue>
class TaskRunner
{
	TaskRunner();
	std::queue<std::string> taskList;
	void start();
	void popTask(std::string taskName);
};

