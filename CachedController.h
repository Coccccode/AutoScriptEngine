#pragma once

#include "Controller.h"
#include <memory>
#include <string>

class CachedController : public Controller
{
public:
	explicit CachedController(std::unique_ptr<Controller> inner);
	CachedController(std::unique_ptr<Controller> inner, std::string cacheKey);

	cv::Mat captureScreen() override;
	cv::Mat captureFresh();
	void click(int x, int y) override;
	void doubleClick(int x, int y) override;
	void longClick(int x, int y, int durationMs = 1000) override;
	void swipe(int x1, int y1, int x2, int y2, int durationMs = 300) override;
	void drag(int x1, int y1, int x2, int y2, int durationMs = 500) override;
	int screenWidth() override;
	int screenHeight() override;

private:
	std::unique_ptr<Controller> m_inner;
	std::string m_cacheKey;
};
