#include "CaptureService.h"

#include "CachedController.h"

#include <algorithm>
#include <chrono>
#include <iostream>

CaptureService::CaptureService()
	: m_running(false)
{
}

CaptureService::~CaptureService()
{
	stop();
}

void CaptureService::start(CachedController* controller, int fps)
{
	if (controller == nullptr) return;

	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_running.load()) return;

	fps = std::max(1, std::min(60, fps));
	m_running.store(true);
	m_thread = std::thread(&CaptureService::loop, this, controller, fps);
}

void CaptureService::stop()
{
	std::thread threadToJoin;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_running.load() && !m_thread.joinable()) return;
		m_running.store(false);
		if (m_thread.joinable())
		{
			threadToJoin = std::move(m_thread);
		}
	}

	if (threadToJoin.joinable())
	{
		threadToJoin.join();
	}
}

bool CaptureService::isRunning() const
{
	return m_running.load();
}

void CaptureService::loop(CachedController* controller, int fps)
{
	using clock = std::chrono::steady_clock;
	const auto interval = std::chrono::milliseconds(1000 / fps);
	auto nextFrame = clock::now();

	while (m_running.load())
	{
		try
		{
			controller->captureFresh();
		}
		catch (const std::exception& ex)
		{
			std::cerr << "capture thread error: " << ex.what() << std::endl;
		}
		catch (...)
		{
			std::cerr << "capture thread error: unknown exception" << std::endl;
		}

		nextFrame += interval;
		std::this_thread::sleep_until(nextFrame);
		if (clock::now() > nextFrame + interval)
		{
			nextFrame = clock::now();
		}
	}
}
