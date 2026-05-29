#include "TaskControl.h"
#include <unordered_map>
#include <mutex>

namespace
{
	std::mutex g_stopMutex;
	std::unordered_map<std::string, bool> g_stopMap;
}

namespace TaskControl
{
	void requestStop(const std::string& instanceKey)
	{
		std::lock_guard<std::mutex> lock(g_stopMutex);
		g_stopMap[instanceKey] = true;
	}

	void resetStop(const std::string& instanceKey)
	{
		std::lock_guard<std::mutex> lock(g_stopMutex);
		g_stopMap[instanceKey] = false;
	}

	bool shouldStop(const std::string& instanceKey)
	{
		std::lock_guard<std::mutex> lock(g_stopMutex);
		return g_stopMap[instanceKey];
	}
}