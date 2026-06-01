#include "CachedController.h"
#include "SharedFrameCache.h"

CachedController::CachedController(std::unique_ptr<Controller> inner)
	: CachedController(std::move(inner), "default")
{
}

CachedController::CachedController(std::unique_ptr<Controller> inner, std::string cacheKey)
	: m_inner(std::move(inner)), m_cacheKey(std::move(cacheKey))
{
}

cv::Mat CachedController::captureScreen()
{
	cv::Mat cached;
	if (SharedFrameCache::latest(m_cacheKey, cached, 250))
	{
		return cached;
	}

	return captureFresh();
}

cv::Mat CachedController::captureFresh()
{
	cv::Mat frame = m_inner ? m_inner->captureScreen() : cv::Mat();
	SharedFrameCache::publish(m_cacheKey, frame);
	return frame;
}

void CachedController::click(int x, int y)
{
	if (m_inner) m_inner->click(x, y);
}

void CachedController::doubleClick(int x, int y)
{
	if (m_inner) m_inner->doubleClick(x, y);
}

void CachedController::longClick(int x, int y, int durationMs)
{
	if (m_inner) m_inner->longClick(x, y, durationMs);
}

void CachedController::swipe(int x1, int y1, int x2, int y2, int durationMs)
{
	if (m_inner) m_inner->swipe(x1, y1, x2, y2, durationMs);
}

void CachedController::drag(int x1, int y1, int x2, int y2, int durationMs)
{
	if (m_inner) m_inner->drag(x1, y1, x2, y2, durationMs);
}

int CachedController::screenWidth()
{
	return m_inner ? m_inner->screenWidth() : 0;
}

int CachedController::screenHeight()
{
	return m_inner ? m_inner->screenHeight() : 0;
}
