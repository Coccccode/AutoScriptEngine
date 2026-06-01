#pragma once

#include <atomic>
#include <mutex>
#include <thread>

class CachedController;

class CaptureService
{
public:
	CaptureService();
	~CaptureService();

	void start(CachedController* controller, int fps = 30);
	void stop();
	bool isRunning() const;

private:
	void loop(CachedController* controller, int fps);

	std::atomic<bool> m_running;
	std::thread m_thread;
	mutable std::mutex m_mutex;
};
