#include "SharedFrameCache.h"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <unordered_map>

namespace
{
	constexpr size_t kRingSize = 4;
	const std::string kDefaultKey = "default";

	struct FrameRing
	{
		std::mutex mutex;
		std::condition_variable ready;
		std::array<cv::Mat, kRingSize> ring;
		size_t writeIndex = 0;
		size_t latestIndex = 0;
		uint64_t sequence = 0;
		std::chrono::steady_clock::time_point latestTime;
		std::atomic<bool> taskRunning{ false };
	};

	std::mutex g_mapMutex;
	std::unordered_map<std::string, std::shared_ptr<FrameRing>> g_rings;

	std::shared_ptr<FrameRing> ringFor(const std::string& key)
	{
		std::lock_guard<std::mutex> lock(g_mapMutex);
		auto it = g_rings.find(key.empty() ? kDefaultKey : key);
		if (it != g_rings.end()) return it->second;

		auto ring = std::make_shared<FrameRing>();
		g_rings[key.empty() ? kDefaultKey : key] = ring;
		return ring;
	}
}

namespace SharedFrameCache
{
	void publish(const cv::Mat& frame)
	{
		publish(kDefaultKey, frame);
	}

	void publish(const std::string& key, const cv::Mat& frame)
	{
		if (frame.empty()) return;

		auto cache = ringFor(key);
		{
			std::lock_guard<std::mutex> lock(cache->mutex);
			cache->ring[cache->writeIndex] = frame;
			cache->latestIndex = cache->writeIndex;
			cache->writeIndex = (cache->writeIndex + 1) % kRingSize;
			cache->latestTime = std::chrono::steady_clock::now();
			++cache->sequence;
		}
		cache->ready.notify_all();
	}

	bool latest(cv::Mat& frame, int maxAgeMs)
	{
		return latest(kDefaultKey, frame, maxAgeMs);
	}

	bool latest(const std::string& key, cv::Mat& frame, int maxAgeMs)
	{
		auto cache = ringFor(key);
		std::lock_guard<std::mutex> lock(cache->mutex);
		const cv::Mat& latestFrame = cache->ring[cache->latestIndex];
		if (latestFrame.empty()) return false;

		if (maxAgeMs > 0)
		{
			auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - cache->latestTime).count();
			if (age > maxAgeMs) return false;
		}

		frame = latestFrame;
		return true;
	}

	bool waitLatest(cv::Mat& frame, int maxAgeMs, int timeoutMs)
	{
		return waitLatest(kDefaultKey, frame, maxAgeMs, timeoutMs);
	}

	bool waitLatest(const std::string& key, cv::Mat& frame, int maxAgeMs, int timeoutMs)
	{
		auto cache = ringFor(key);
		std::unique_lock<std::mutex> lock(cache->mutex);
		if (timeoutMs > 0 && cache->ring[cache->latestIndex].empty())
		{
			cache->ready.wait_for(lock, std::chrono::milliseconds(timeoutMs), [&] {
				return !cache->ring[cache->latestIndex].empty();
			});
		}

		const cv::Mat& latestFrame = cache->ring[cache->latestIndex];
		if (latestFrame.empty()) return false;

		if (maxAgeMs > 0)
		{
			auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
				std::chrono::steady_clock::now() - cache->latestTime).count();
			if (age > maxAgeMs) return false;
		}

		frame = latestFrame;
		return true;
	}

	void clear()
	{
		clear(kDefaultKey);
	}

	void clear(const std::string& key)
	{
		auto cache = ringFor(key);
		std::lock_guard<std::mutex> lock(cache->mutex);
		for (auto& frame : cache->ring)
		{
			frame.release();
		}
		cache->writeIndex = 0;
		cache->latestIndex = 0;
		cache->sequence = 0;
		cache->latestTime = {};
	}

	void setTaskRunning(bool running)
	{
		setTaskRunning(kDefaultKey, running);
	}

	bool isTaskRunning()
	{
		return isTaskRunning(kDefaultKey);
	}

	void setTaskRunning(const std::string& key, bool running)
	{
		ringFor(key)->taskRunning.store(running);
	}

	bool isTaskRunning(const std::string& key)
	{
		return ringFor(key)->taskRunning.load();
	}
}
