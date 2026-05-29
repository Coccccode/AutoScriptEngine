#pragma once

#include <opencv2/core.hpp>

namespace SharedFrameCache
{
	void publish(const cv::Mat& frame);
	bool latest(cv::Mat& frame, int maxAgeMs = 0);
	bool waitLatest(cv::Mat& frame, int maxAgeMs = 0, int timeoutMs = 0);
	void clear();
	void setTaskRunning(bool running);
	bool isTaskRunning();
}
