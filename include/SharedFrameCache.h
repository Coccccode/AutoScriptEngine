#pragma once

#include <opencv2/core.hpp>
#include <string>

namespace SharedFrameCache
{
	void publish(const cv::Mat& frame);
	void publish(const std::string& key, const cv::Mat& frame);
	bool latest(cv::Mat& frame, int maxAgeMs = 0);
	bool latest(const std::string& key, cv::Mat& frame, int maxAgeMs = 0);
	bool waitLatest(cv::Mat& frame, int maxAgeMs = 0, int timeoutMs = 0);
	bool waitLatest(const std::string& key, cv::Mat& frame, int maxAgeMs = 0, int timeoutMs = 0);
	void clear();
	void clear(const std::string& key);
	void setTaskRunning(bool running);
	void setTaskRunning(const std::string& key, bool running);
	bool isTaskRunning();
	bool isTaskRunning(const std::string& key);
}
