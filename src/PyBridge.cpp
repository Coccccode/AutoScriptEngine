#include <pybind11/pybind11.h>

#include <Windows.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <iostream>
#include <fstream>
#include <memory>
#include <mutex>
#include <regex>
#include <sstream>
#include <streambuf>
#include <string>
#include <unordered_map>
#include <vector>

#include "AdbController.h"
#include "CachedController.h"
#include "CaptureService.h"
#include "Controller.h"
#include "SharedFrameCache.h"
#include "TaskRunner.h"
#include "TaskControl.h"
#include "WindowConfig.h"
#include "WindowController.h"
#include "WindowsGraphicsCapture.h"

namespace py = pybind11;

namespace
{
	struct SharedDevice
	{
		std::string configJson;
		std::unique_ptr<WindowConfig> config;
		std::unique_ptr<CachedController> controller;
		std::unique_ptr<CaptureService> captureService;
	};

	std::mutex g_sharedDeviceMutex;
	std::unordered_map<std::string, std::unique_ptr<SharedDevice>> g_sharedDevices;

	void bridgeDebug(const std::string& message)
	{
		(void)message;
	}

	std::string readTextFile(const std::string& path)
	{
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open()) return "";
		return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}

	std::string decodeJsonEscapes(const std::string& value);

	std::string jsonStringValue(const std::string& json, const std::string& key, const std::string& fallback)
	{
		std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
		std::smatch match;
		if (!std::regex_search(json, match, pattern)) return fallback;
		return decodeJsonEscapes(match[1].str());
	}

	void appendUtf8(std::string& out, unsigned int codepoint)
	{
		if (codepoint <= 0x7F)
		{
			out.push_back((char)codepoint);
		}
		else if (codepoint <= 0x7FF)
		{
			out.push_back((char)(0xC0 | (codepoint >> 6)));
			out.push_back((char)(0x80 | (codepoint & 0x3F)));
		}
		else
		{
			out.push_back((char)(0xE0 | (codepoint >> 12)));
			out.push_back((char)(0x80 | ((codepoint >> 6) & 0x3F)));
			out.push_back((char)(0x80 | (codepoint & 0x3F)));
		}
	}

	std::string decodeJsonEscapes(const std::string& value)
	{
		std::string out;
		for (size_t i = 0; i < value.size(); ++i)
		{
			if (value[i] == '\\' && i + 1 < value.size())
			{
				char next = value[++i];
				if (next == 'u' && i + 4 < value.size())
				{
					std::string hex = value.substr(i + 1, 4);
					char* end = nullptr;
					unsigned long code = std::strtoul(hex.c_str(), &end, 16);
					if (end != nullptr && *end == '\0')
					{
						appendUtf8(out, (unsigned int)code);
						i += 4;
					}
				}
				else if (next == 'n') out.push_back('\n');
				else if (next == 'r') out.push_back('\r');
				else if (next == 't') out.push_back('\t');
				else if (next == '"' || next == '\\' || next == '/') out.push_back(next);
				else
				{
					out.push_back('\\');
					out.push_back(next);
				}
			}
			else
			{
				out.push_back(value[i]);
			}
		}
		return out;
	}

	int jsonIntValue(const std::string& json, const std::string& key, int fallback)
	{
		std::regex pattern("\"" + key + "\"\\s*:\\s*(-?\\d+)");
		std::smatch match;
		if (!std::regex_search(json, match, pattern)) return fallback;
		return std::atoi(match[1].str().c_str());
	}

	std::wstring utf8ToWideText(const std::string& text)
	{
		if (text.empty()) return L"";
		int len = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
		if (len <= 0) return L"";
		std::wstring wide(len - 1, L'\0');
		MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), len);
		return wide;
	}

	py::object decodeWindowsText(const std::string& text)
	{
		PyObject* obj = PyUnicode_Decode(text.data(), (Py_ssize_t)text.size(), "mbcs", "replace");
		if (obj == nullptr)
		{
			PyErr_Clear();
			return py::str(text);
		}
		return py::reinterpret_steal<py::object>(obj);
	}

	class PythonLogBuffer : public std::streambuf
	{
	public:
		explicit PythonLogBuffer(py::object callback)
			: m_callback(std::move(callback))
		{
		}

		~PythonLogBuffer() override
		{
			sync();
		}

	protected:
		int overflow(int ch) override
		{
			if (ch == traits_type::eof()) return traits_type::not_eof(ch);
			if (ch == '\n')
			{
				emit();
			}
			else if (ch != '\r')
			{
				m_line.push_back((char)ch);
			}
			return ch;
		}

		std::streamsize xsputn(const char* s, std::streamsize count) override
		{
			for (std::streamsize i = 0; i < count; ++i)
			{
				overflow((unsigned char)s[i]);
			}
			return count;
		}

		int sync() override
		{
			emit();
			return 0;
		}

	private:
		void emit()
		{
			if (m_line.empty()) return;
			py::gil_scoped_acquire gil;
			try
			{
				m_callback(decodeWindowsText(m_line));
			}
			catch (const py::error_already_set&)
			{
				PyErr_Clear();
			}
			m_line.clear();
		}

		py::object m_callback;
		std::string m_line;
	};

	std::unique_ptr<Controller> createController(WindowConfig& config)
	{
		if (config.controlType == AdbType)
		{
			return std::make_unique<AdbController>(config.adbPath, config.deviceSerial);
		}
		return std::make_unique<WindowController>(&config);
	}

	SharedDevice* sharedDevice(const std::string& configPath)
	{
		std::lock_guard<std::mutex> lock(g_sharedDeviceMutex);
		std::string key = configPath.empty() ? "configs/config.json" : configPath;
		std::string configJson = readTextFile(configPath);
		auto& slot = g_sharedDevices[key];
		if (!slot)
		{
			slot = std::make_unique<SharedDevice>();
			slot->captureService = std::make_unique<CaptureService>();
		}
		if (slot->controller && slot->configJson == configJson)
		{
			return slot.get();
		}

		slot->captureService->stop();
		slot->controller.reset();
		slot->config = std::make_unique<WindowConfig>(configPath);
		slot->controller = std::make_unique<CachedController>(createController(*slot->config), key);
		slot->configJson = configJson;
		SharedFrameCache::clear(key);
		return slot.get();
	}

	CachedController* sharedController(const std::string& configPath)
	{
		SharedDevice* device = sharedDevice(configPath);
		return device ? device->controller.get() : nullptr;
	}

	bool matchTextOrClass(HWND hwnd, const std::wstring& value)
	{
		if (value.empty()) return false;
		wchar_t buf[256] = { 0 };
		GetWindowTextW(hwnd, buf, 256);
		if (wcscmp(buf, value.c_str()) == 0) return true;
		GetClassNameW(hwnd, buf, 256);
		return _wcsicmp(buf, value.c_str()) == 0;
	}

	HWND findDescendantWindow(HWND parent, const std::wstring& target)
	{
		HWND child = FindWindowExW(parent, nullptr, nullptr, nullptr);
		while (child != nullptr)
		{
			HWND nested = findDescendantWindow(child, target);
			if (nested != nullptr) return nested;
			if (matchTextOrClass(child, target)) return child;
			child = FindWindowExW(parent, child, nullptr, nullptr);
		}
		return nullptr;
	}

	struct TopWindowSearch
	{
		const wchar_t* title;
		const wchar_t* className;
		HWND hwnd;
	};

	BOOL CALLBACK enumTopWindowProc(HWND candidate, LPARAM lParam)
	{
		TopWindowSearch* search = reinterpret_cast<TopWindowSearch*>(lParam);
		if (!IsWindowVisible(candidate)) return TRUE;

		wchar_t title[256] = { 0 };
		wchar_t className[256] = { 0 };
		GetWindowTextW(candidate, title, 256);
		GetClassNameW(candidate, className, 256);

		if (wcscmp(title, search->title) == 0 &&
			(search->className == nullptr || _wcsicmp(className, search->className) == 0))
		{
			search->hwnd = candidate;
			return FALSE;
		}
		return TRUE;
	}

	HWND findTopWindow(const std::wstring& title)
	{
		std::vector<std::wstring> titles;
		if (!title.empty()) titles.push_back(title);
		titles.push_back(L"MuMu安卓设备");

		for (const auto& item : titles)
		{
			TopWindowSearch titleSearch = { item.c_str(), nullptr, nullptr };
			EnumWindows(enumTopWindowProc, reinterpret_cast<LPARAM>(&titleSearch));
			if (titleSearch.hwnd != nullptr) return titleSearch.hwnd;
		}
		return nullptr;
	}

	cv::Mat captureWindowByBitBlt(HWND topHwnd, HWND targetHwnd)
	{
		if (topHwnd == nullptr || !IsWindow(topHwnd)) return cv::Mat();
		if (targetHwnd == nullptr || !IsWindow(targetHwnd)) targetHwnd = topHwnd;

		RECT rect;
		if (!GetWindowRect(targetHwnd, &rect)) return cv::Mat();
		int width = rect.right - rect.left;
		int height = rect.bottom - rect.top;
		if (width <= 0 || height <= 0) return cv::Mat();

		HDC screen = GetDC(nullptr);
		HDC mem = CreateCompatibleDC(screen);
		HBITMAP bmp = CreateCompatibleBitmap(screen, width, height);
		HGDIOBJ old = SelectObject(mem, bmp);

		BitBlt(mem, 0, 0, width, height, screen, rect.left, rect.top, SRCCOPY | CAPTUREBLT);

		cv::Mat bgra(height, width, CV_8UC4);
		BITMAPINFOHEADER header = { sizeof(header), width, -height, 1, 32, BI_RGB };
		GetDIBits(mem, bmp, 0, height, bgra.data, reinterpret_cast<BITMAPINFO*>(&header), DIB_RGB_COLORS);

		SelectObject(mem, old);
		DeleteObject(bmp);
		DeleteDC(mem);
		ReleaseDC(nullptr, screen);

		cv::Mat image;
		cv::cvtColor(bgra, image, cv::COLOR_BGRA2BGR);
		return image;
	}

	py::bytes matToPngBytes(const cv::Mat& image)
	{
		if (image.empty()) return py::bytes();

		std::vector<uchar> buffer;
		std::vector<int> params = { cv::IMWRITE_PNG_COMPRESSION, 3 };
		if (!cv::imencode(".png", image, buffer, params))
		{
			return py::bytes();
		}
		return py::bytes(reinterpret_cast<const char*>(buffer.data()), buffer.size());
	}

	cv::Mat resizeToFit(const cv::Mat& image, int maxWidth, int maxHeight)
	{
		if (image.empty() || maxWidth <= 0 || maxHeight <= 0) return image;
		double scale = std::min((double)maxWidth / image.cols, (double)maxHeight / image.rows);
		if (scale >= 1.0) return image;

		cv::Mat resized;
		cv::resize(image, resized, cv::Size((int)(image.cols * scale), (int)(image.rows * scale)), 0, 0, cv::INTER_AREA);
		return resized;
	}

	void startCapture(const std::string& configPath, int fps)
	{
		SharedDevice* device = sharedDevice(configPath);
		if (device && device->captureService)
		{
			device->captureService->start(device->controller.get(), fps <= 0 ? 30 : fps);
		}
	}

	void stopCapture(const std::string& configPath)
	{
		if (!SharedFrameCache::isTaskRunning(configPath))
		{
			SharedDevice* device = sharedDevice(configPath);
			if (device && device->captureService)
			{
				device->captureService->stop();
			}
		}
	}

	bool isCaptureRunning(const std::string& configPath)
	{
		SharedDevice* device = sharedDevice(configPath);
		return device && device->captureService && device->captureService->isRunning();
	}

	bool isTaskRunning(const std::string& configPath)
	{
		return SharedFrameCache::isTaskRunning(configPath);
	}

	void runTask(const std::string& taskName, const std::string& configPath, py::object logCallback)
	{
		TaskControl::resetStop();
		if (logCallback.is_none())
		{
			CachedController* device = sharedController(configPath);
			startCapture(configPath, 30);
			SharedFrameCache::setTaskRunning(configPath, true);
			try
			{
				TaskRunner runner(device);
				py::gil_scoped_release release;
				runner.runTask(taskName);
				SharedFrameCache::setTaskRunning(configPath, false);
			}
			catch (...)
			{
				SharedFrameCache::setTaskRunning(configPath, false);
				throw;
			}
			return;
		}

		PythonLogBuffer outBuffer(logCallback);
		PythonLogBuffer errBuffer(logCallback);
		std::streambuf* oldOut = std::cout.rdbuf(&outBuffer);
		std::streambuf* oldErr = std::cerr.rdbuf(&errBuffer);
		try
		{
			CachedController* device = sharedController(configPath);
			startCapture(configPath, 30);
			SharedFrameCache::setTaskRunning(configPath, true);
			TaskRunner runner(device);
			{
				py::gil_scoped_release release;
				runner.runTask(taskName);
			}
			SharedFrameCache::setTaskRunning(configPath, false);
			std::cout.flush();
			std::cerr.flush();
		}
		catch (...)
		{
			SharedFrameCache::setTaskRunning(configPath, false);
			std::cout.rdbuf(oldOut);
			std::cerr.rdbuf(oldErr);
			throw;
		}
		std::cout.rdbuf(oldOut);
		std::cerr.rdbuf(oldErr);
	}

	void requestStop()
	{
		TaskControl::requestStop();
	}

	py::bytes latestPngFor(const std::string& configPath, int maxWidth, int maxHeight, int maxAgeMs)
	{
		cv::Mat image;
		if (!SharedFrameCache::latest(configPath, image, maxAgeMs))
		{
			return py::bytes();
		}
		return matToPngBytes(resizeToFit(image, maxWidth, maxHeight));
	}

	py::bytes latestPng(int maxWidth, int maxHeight, int maxAgeMs)
	{
		return latestPngFor("", maxWidth, maxHeight, maxAgeMs);
	}

	py::bytes capturePng(const std::string& configPath, int maxWidth, int maxHeight)
	{
		bridgeDebug("capturePng begin");
		cv::Mat image;
		if (!isCaptureRunning(configPath))
		{
			startCapture(configPath, 30);
		}

		if (!SharedFrameCache::waitLatest(configPath, image, 3000, 500))
		{
			CachedController* device = nullptr;
			{
				py::gil_scoped_release release;
				device = sharedController(configPath);
				image = device ? device->captureFresh() : cv::Mat();
			}
		}
		return matToPngBytes(resizeToFit(image, maxWidth, maxHeight));

		std::string configJson = readTextFile(configPath);
		int controlType = jsonIntValue(configJson, "controlType", (int)WindowApiType);
		std::string adbPath = jsonStringValue(configJson, "adbPath", "adb");
		std::string deviceSerial = jsonStringValue(configJson, "deviceSerial", "");
		std::string captureBackend = jsonStringValue(configJson, "captureBackend", "wgc");
		std::wstring windowName = utf8ToWideText(jsonStringValue(configJson, "windowName", "MuMu安卓设备"));
		std::wstring targetWindowName = utf8ToWideText(jsonStringValue(configJson, "targetWindowName", "MuMuNxDevice"));
		bridgeDebug("config loaded without jsoncpp");
		image.release();
		if (controlType == (int)AdbType)
		{
			bridgeDebug("adb capture begin");
			AdbController controller(adbPath, deviceSerial);
			py::gil_scoped_release release;
			image = controller.captureScreen();
			bridgeDebug("adb capture done");
		}
		else
		{
			bridgeDebug("window find begin");
			HWND top = findTopWindow(windowName);
			bridgeDebug(top == nullptr ? "top null" : "top found");
			HWND target = top;
			if (top != nullptr && !targetWindowName.empty())
			{
				HWND child = findDescendantWindow(top, targetWindowName);
				if (child != nullptr) target = child;
			}
			bridgeDebug(target == nullptr ? "target null" : "target found");
			if (captureBackend == "wgc" && target != nullptr)
			{
				bridgeDebug("wgc capture begin");
				WindowsGraphicsCapture wgc;
				py::gil_scoped_release release;
				image = wgc.capture(target);
				if (image.empty() && target != top)
				{
					image = wgc.capture(top);
				}
				bridgeDebug(image.empty() ? "wgc capture empty" : "wgc capture done");
			}
			if (image.empty())
			{
				bridgeDebug("gdi fallback begin");
				py::gil_scoped_release release;
				image = captureWindowByBitBlt(top, target);
			}
			bridgeDebug(image.empty() ? "window capture empty" : "window capture done");
		}
		bridgeDebug("encode begin");
		return matToPngBytes(resizeToFit(image, maxWidth, maxHeight));
	}
}

PYBIND11_MODULE(learnopencv_py, m)
{
	m.doc() = "pybind11 bindings for learnOpencv automation";
	m.def("run_task", &runTask, py::arg("task_name"), py::arg("config_path") = "configs/config.json", py::arg("log_callback") = py::none());
	m.def("request_stop", &requestStop);
	m.def("start_capture", &startCapture, py::arg("config_path") = "configs/config.json", py::arg("fps") = 30);
	m.def("stop_capture", &stopCapture);
	m.def("is_capture_running", &isCaptureRunning);
	m.def("is_task_running", &isTaskRunning);
	m.def("latest_png_for", &latestPngFor, py::arg("config_path") = "configs/config.json", py::arg("max_width") = 0, py::arg("max_height") = 0, py::arg("max_age_ms") = 0);
	m.def("latest_png", &latestPng, py::arg("max_width") = 0, py::arg("max_height") = 0, py::arg("max_age_ms") = 0);
	m.def("capture_png", &capturePng, py::arg("config_path") = "configs/config.json", py::arg("max_width") = 0, py::arg("max_height") = 0);
}
