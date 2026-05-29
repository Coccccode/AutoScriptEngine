#pragma once
#include <string>

namespace TaskControl
{
	// 注意这里必须是 const std::string& 
	void requestStop(const std::string& instanceKey = "default");
	void resetStop(const std::string& instanceKey = "default");
	bool shouldStop(const std::string& instanceKey = "default");
}