#pragma once
#include <string>
#include <json/json.h>
#include <fstream>

class TaskRunner
{
	TaskRunner(std::string taskName);
	void start();
};

