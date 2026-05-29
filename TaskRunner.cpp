#include "TaskRunner.h"
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <direct.h>
#include <sys/stat.h>
#include "CommonMath.h"
#include "TaskControl.h"
namespace
{
	std::string consoleTextFromUtf8(const std::string& text)
	{
		if (text.empty()) return text;

		int wideLen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
		if (wideLen <= 0) return text;

		std::vector<wchar_t> wide(wideLen);
		MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, wide.data(), wideLen);

		UINT outputCp = GetConsoleOutputCP();
		if (outputCp == 0) outputCp = CP_ACP;

		int localLen = WideCharToMultiByte(outputCp, 0, wide.data(), -1, NULL, 0, NULL, NULL);
		if (localLen <= 0) return text;

		std::vector<char> local(localLen);
		WideCharToMultiByte(outputCp, 0, wide.data(), -1, local.data(), localLen, NULL, NULL);
		return std::string(local.data());
	}
}

TaskRunner::TaskRunner()
	: TaskRunner(nullptr)
{
}

TaskRunner::TaskRunner(Controller* externalController)
{
	cvapi = new OpencvAPI();
	config = new WindowConfig("config.json");
	controller = externalController;
	m_ownsController = externalController == nullptr;
	mouse = new MouseController();
	ocr = new OcrApi();
	m_imageIndex = 0;
	m_visibleScrollIndex = 0;

	if (controller != NULL)
	{
		std::cout << "Using shared device backend" << std::endl;
	}
	else if (config->controlType == WindowApiType)
	{
		controller = new WindowController(config);
		std::cout << "Using Windows API backend" << std::endl;
	}
	else if (config->controlType == AdbType)
	{
		controller = new AdbController(config->adbPath, config->deviceSerial);
		std::cout << "Using ADB backend" << std::endl;
	}
}

TaskRunner::~TaskRunner()
{
	delete cvapi;
	if (m_ownsController) delete controller;
	delete config;
	delete mouse;
	delete ocr;
}

void TaskRunner::addTask(std::string taskName)
{
	taskList.push(taskName);
}

void TaskRunner::start()
{
	while (1)
	{
		if (taskList.empty())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			continue;
		}
		std::string taskName = taskList.front();
		runTask(taskName);
		taskList.pop();
	}
}

void TaskRunner::popTask(std::string taskName)
{
	if (!taskList.empty() && taskList.front() == taskName)
	{
		taskList.pop();
	}
}

void TaskRunner::saveCaptureImage(const cv::Mat& image, const std::string& stepName)
{
	if (image.empty()) return;

	_mkdir("screenshots");

	char filename[256];
	sprintf_s(filename, "screenshots/%04d_%s.png", m_imageIndex, stepName.c_str());
	cv::imwrite(filename, image);
	std::cout << "Screenshot saved: " << filename << std::endl;
	m_imageIndex++;
}

void TaskRunner::runTask(std::string taskName)
{
	if (controller == NULL)
	{
		std::cout << "controller not found" << std::endl;
		return;
	}

	cv::Mat testImg = controller->captureScreen();
	if (testImg.empty() && config->controlType == WindowApiType)
	{
		std::cout << "window not found" << std::endl;
		return;
	}

	std::string filepath = taskName + ".json";
	Json::Value root = readTaskJson(filepath);
	if (root.isNull() || !root.isMember(taskName))
	{
		std::cerr << "failed to load task: " << taskName << std::endl;
		return;
	}

	Json::Value nextStep = root[taskName]["next"];
	Json::Value currentStep;
	int i = 0;
	int maxIterations = 1000;

	while (maxIterations-- > 0)
	{
		if (TaskControl::shouldStop())
		{
			std::cout << "task stopped by request" << std::endl;
			break;
		}

		if (nextStep.size() == 0)
		{
			std::cout << "task completed" << std::endl;
			break;
		}
		if (i >= (int)nextStep.size())
		{
			i = 0;
		}

		std::string stepName = nextStep[i].asString();
		if (!root.isMember(stepName))
		{
			std::cerr << "step not found: " << stepName << std::endl;
			i++;
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			continue;
		}

		currentStep = root[stepName];
		std::string action = currentStep.get("action", "null").asString();
		int sleepTime = currentStep.get("delay", 0).asInt();

		if (action == "null")
		{
			if (sleepTime > 0)
			{
				std::this_thread::sleep_for(std::chrono::seconds(sleepTime));
			}
			nextStep = currentStep["next"];
			i = 0;
			continue;
		}

		if (action == "click")
		{
			handleClickAction(currentStep, nextStep, i, sleepTime);
		}
		else if (action == "detect" || action == "waitFor")
		{
			handleDetectAction(currentStep, nextStep, i);
		}
		else if (action == "doubleClick")
		{
			handleDoubleClickAction(currentStep, nextStep, i, sleepTime);
		}
		else if (action == "longClick")
		{
			handleLongClickAction(currentStep, nextStep, i, sleepTime);
		}
		else if (action == "swipe")
		{
			handleSwipeAction(currentStep, nextStep, i, sleepTime);
		}
		else if (action == "ocr")
		{
			handleOcrAction(currentStep, nextStep, i, sleepTime);
		}
		else if (action == "prioritySearch")
		{
			handlePrioritySearchAction(currentStep, nextStep, i, sleepTime);
		}
		else if (action == "collectPrioritySearch")
		{
			handleCollectPrioritySearchAction(currentStep, nextStep, i, sleepTime);
		}
		else if (action == "selectPriorityRecord")
		{
			handleSelectPriorityRecordAction(currentStep, nextStep, i, sleepTime);
		}
		else
		{
			std::cerr << "unknown action: " << action << ", skip step: " << stepName << std::endl;
			i++;
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	}

	if (maxIterations <= 0)
	{
		std::cerr << "task exceeded max iterations, stopped" << std::endl;
	}
}

void TaskRunner::handleClickAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime)
{
	cv::Point clickPoint;
	if (!waitResolveClickPoint(currentStep, "click_match", clickPoint))
	{
		i++;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		return;
	}

	sleepBeforeClick(currentStep, sleepTime);
	controller->click(clickPoint.x, clickPoint.y);
	sleepAfterClick(currentStep);

	cv::Mat afterClickImg = controller->captureScreen();
	saveCaptureImage(afterClickImg, "click_done");

	i = 0;
	nextStep = currentStep["next"];
}
void TaskRunner::handleDetectAction(Json::Value& currentStep, Json::Value& nextStep, int& i)
{
	cv::Point foundPoint;
	if (!waitResolveClickPoint(currentStep, "detect_match", foundPoint))
	{
		i++;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		return;
	}

	std::cout << "detect matched at (" << foundPoint.x << "," << foundPoint.y << ")" << std::endl;
	sleepAfterClick(currentStep);
	i = 0;
	nextStep = currentStep["next"];
}
bool TaskRunner::waitResolveClickPoint(Json::Value& currentStep, const std::string& imageName, cv::Point& clickPoint)
{
	int timeoutMs = currentStep.get("recognitionTimeoutMs", currentStep.get("timeoutMs", -1)).asInt();
	if (timeoutMs < 0)
	{
		timeoutMs = currentStep.get("recognitionTimeout", currentStep.get("timeout", 0)).asInt() * 1000;
	}
	int intervalMs = currentStep.get("recognitionIntervalMs", 500).asInt();
	if (intervalMs < 50) intervalMs = 50;

	auto start = std::chrono::steady_clock::now();
	int attempt = 0;
	while (true)
	{
		attempt++;
		if (resolveClickPoint(currentStep, imageName, clickPoint))
		{
			return true;
		}

		if (timeoutMs <= 0)
		{
			return false;
		}

		auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
		if (elapsed >= timeoutMs)
		{
			std::cerr << "recognition timeout after " << elapsed << "ms, attempts=" << attempt << std::endl;
			return false;
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
	}
}
bool TaskRunner::resolveClickPoint(Json::Value& currentStep, const std::string& imageName, cv::Point& clickPoint)
{
	std::string recogType = currentStep.get("recognization", "TemplateMatch").asString();

	if (recogType == "DirectHint" || recogType == "directHint" || recogType == "direct")
	{
		return resolveDirectHintPoint(currentStep, clickPoint);
	}

	cv::Mat captureImg = controller->captureScreen();
	if (captureImg.empty())
	{
		std::cerr << "capture image is empty" << std::endl;
		return false;
	}

	cv::Point roiOffset(0, 0);
	cv::Mat recogImg = applyRecognitionRoi(currentStep, captureImg, roiOffset);
	if (recogImg.empty())
	{
		return false;
	}

	bool ok = false;
	if (recogType == "TemplateMatch" || recogType == "template" || recogType == "Template")
	{
		ok = resolveTemplateMatchPoint(currentStep, recogImg, clickPoint);
	}
	else if (recogType == "OCR" || recogType == "Ocr" || recogType == "ocr")
	{
		ok = resolveOcrPoint(currentStep, recogImg, clickPoint);
	}
	else
	{
		std::cerr << "unknown recognization: " << recogType << std::endl;
		return false;
	}

	if (ok)
	{
		clickPoint.x += roiOffset.x;
		clickPoint.y += roiOffset.y;
		saveCaptureImage(recogImg, imageName);
	}
	return ok;
}

cv::Mat TaskRunner::applyRecognitionRoi(Json::Value& currentStep, const cv::Mat& image, cv::Point& offset)
{
	offset = cv::Point(0, 0);
	if (image.empty() || !currentStep.isMember("roi"))
	{
		return image;
	}

	const Json::Value& roi = currentStep["roi"];
	int x = roi.isMember("xRatio") ? (int)(roi["xRatio"].asDouble() * image.cols) : roi.get("x", 0).asInt();
	int y = roi.isMember("yRatio") ? (int)(roi["yRatio"].asDouble() * image.rows) : roi.get("y", 0).asInt();
	int width = roi.isMember("widthRatio") ? (int)(roi["widthRatio"].asDouble() * image.cols) : roi.get("width", image.cols - x).asInt();
	int height = roi.isMember("heightRatio") ? (int)(roi["heightRatio"].asDouble() * image.rows) : roi.get("height", image.rows - y).asInt();

	cv::Rect bounds(0, 0, image.cols, image.rows);
	cv::Rect rect(x, y, width, height);
	rect = rect & bounds;
	if (rect.width <= 0 || rect.height <= 0)
	{
		std::cerr << "invalid roi" << std::endl;
		return cv::Mat();
	}

	offset = cv::Point(rect.x, rect.y);
	std::cout << "ROI: x=" << rect.x << " y=" << rect.y << " w=" << rect.width << " h=" << rect.height << std::endl;
	return image(rect).clone();
}

bool TaskRunner::resolveTemplateMatchPoint(Json::Value& currentStep, const cv::Mat& captureImg, cv::Point& clickPoint)
{
	std::string templateImgPath = currentStep["template"].asString();
	cv::Mat templateImg = cv::imread(templateImgPath);
	if (templateImg.empty())
	{
		std::cerr << "failed to load template image: " << templateImgPath << std::endl;
		return false;
	}

	double threshold = currentStep.get("threshold", 0.89).asDouble();
	int matchType = currentStep.get("matchType", (int)cv::TM_CCORR_NORMED).asInt();
	bool isMatch = false;
	clickPoint = cvapi->TemplateMatch(captureImg.clone(), templateImg, threshold, matchType, isMatch);
	return isMatch;
}

bool TaskRunner::resolveOcrPoint(Json::Value& currentStep, const cv::Mat& captureImg, cv::Point& clickPoint)
{
	if (!ensureOcrRunning())
	{
		return false;
	}

	std::string targetText = currentStep.get("target", currentStep.get("text", "")).asString();
	std::string ocrConfig = currentStep.get("ocrConfig", "config_chinese.txt").asString();
	double minScore = currentStep.get("minScore", 0.0).asDouble();

	auto results = ocr->recognize(captureImg, ocrConfig);
	if (targetText.empty())
	{
		return !results.empty();
	}

	std::cout << "OCR target: [" << consoleTextFromUtf8(targetText) << "]" << std::endl;

	for (const auto& r : results)
	{
		std::cout << "OCR: [" << consoleTextFromUtf8(r.text) << "] score=" << r.score
			<< " pos=(" << r.x << "," << r.y << ")" << std::endl;

		if (r.score >= minScore && r.text.find(targetText) != std::string::npos)
		{
			clickPoint = cv::Point(r.x + r.width / 2, r.y + r.height / 2);
			std::cout << "OCR matched: [" << consoleTextFromUtf8(r.text) << "] click=(" << clickPoint.x << "," << clickPoint.y << ")" << std::endl;
			return true;
		}
	}
	return false;
}

bool TaskRunner::resolveDirectHintPoint(Json::Value& currentStep, cv::Point& clickPoint)
{
	if (currentStep.get("random", false).asBool())
	{
		int x1 = currentStep.get("x1", 0).asInt();
		int y1 = currentStep.get("y1", 0).asInt();
		int x2 = currentStep.get("x2", x1).asInt();
		int y2 = currentStep.get("y2", y1).asInt();
		if (x2 < x1) std::swap(x1, x2);
		if (y2 < y1) std::swap(y1, y2);
		clickPoint = cv::Point(CommonMath::random(x1, x2), CommonMath::random(y1, y2));
		return true;
	}

	if (currentStep.isMember("point"))
	{
		const Json::Value& point = currentStep["point"];
		clickPoint = cv::Point(point.get("x", 0).asInt(), point.get("y", 0).asInt());
		return true;
	}

	if (currentStep.isMember("x") && currentStep.isMember("y"))
	{
		clickPoint = cv::Point(currentStep["x"].asInt(), currentStep["y"].asInt());
		return true;
	}

	if (currentStep.isMember("clickX") && currentStep.isMember("clickY"))
	{
		clickPoint = cv::Point(currentStep["clickX"].asInt(), currentStep["clickY"].asInt());
		return true;
	}

	std::cerr << "DirectHint requires x/y, clickX/clickY, point{x,y}, or random x1/y1/x2/y2" << std::endl;
	return false;
}

bool TaskRunner::resolvePriorityPoint(Json::Value& currentStep, cv::Point& clickPoint, std::string& matchedName)
{
	if (!currentStep.isMember("candidates") || !currentStep["candidates"].isArray())
	{
		std::cerr << "prioritySearch requires candidates array" << std::endl;
		return false;
	}

	cv::Mat captureImg = controller->captureScreen();
	if (captureImg.empty())
	{
		std::cerr << "capture image is empty" << std::endl;
		return false;
	}

	cv::Point roiOffset(0, 0);
	cv::Mat recogImg = applyRecognitionRoi(currentStep, captureImg, roiOffset);
	if (recogImg.empty())
	{
		return false;
	}

	for (Json::ArrayIndex idx = 0; idx < currentStep["candidates"].size(); ++idx)
	{
		Json::Value candidate = currentStep["candidates"][idx];
		if (!candidate.isMember("template"))
		{
			continue;
		}

		if (!candidate.isMember("threshold") && currentStep.isMember("threshold"))
		{
			candidate["threshold"] = currentStep["threshold"];
		}
		if (!candidate.isMember("matchType") && currentStep.isMember("matchType"))
		{
			candidate["matchType"] = currentStep["matchType"];
		}

		cv::Point localPoint;
		if (resolveTemplateMatchPoint(candidate, recogImg, localPoint))
		{
			clickPoint = cv::Point(localPoint.x + roiOffset.x, localPoint.y + roiOffset.y);
			matchedName = candidate.get("name", candidate["template"].asString()).asString();
			std::cout << "prioritySearch matched: " << matchedName
				<< " click=(" << clickPoint.x << "," << clickPoint.y << ")" << std::endl;
			saveCaptureImage(recogImg, "priority_match");
			return true;
		}
	}

	return false;
}

bool TaskRunner::performConfiguredScroll(Json::Value& currentStep)
{
	Json::Value scroll = currentStep["scroll"];
	int duration = scroll.get("duration", 500).asInt();

	if (scroll.get("type", "").asString() == "sliderTemplate")
	{
		if (!scroll.isMember("template"))
		{
			std::cerr << "sliderTemplate scroll requires template" << std::endl;
			return false;
		}

		Json::Value sliderStep;
		sliderStep["recognization"] = "TemplateMatch";
		sliderStep["template"] = scroll["template"];
		sliderStep["threshold"] = scroll.get("threshold", currentStep.get("threshold", 0.89));
		sliderStep["recognitionTimeoutMs"] = scroll.get("recognitionTimeoutMs", 1500);
		sliderStep["recognitionIntervalMs"] = scroll.get("recognitionIntervalMs", 250);
		if (scroll.isMember("roi"))
		{
			sliderStep["roi"] = scroll["roi"];
		}

		cv::Point sliderPoint;
		if (!waitResolveClickPoint(sliderStep, "slider_match", sliderPoint))
		{
			return false;
		}

		int deltaX = scroll.get("deltaX", 0).asInt();
		int deltaY = scroll.get("deltaY", -300).asInt();
		controller->swipe(sliderPoint.x, sliderPoint.y, sliderPoint.x + deltaX, sliderPoint.y + deltaY, duration);
		return true;
	}

	int screenWidth = controller->screenWidth();
	int screenHeight = controller->screenHeight();
	int x1 = scroll.isMember("x1Ratio")
		? (int)(scroll["x1Ratio"].asDouble() * screenWidth)
		: scroll.get("x1", currentStep.get("x1", screenWidth / 2)).asInt();
	int y1 = scroll.isMember("y1Ratio")
		? (int)(scroll["y1Ratio"].asDouble() * screenHeight)
		: scroll.get("y1", currentStep.get("y1", screenHeight * 3 / 4)).asInt();
	int x2 = scroll.isMember("x2Ratio")
		? (int)(scroll["x2Ratio"].asDouble() * screenWidth)
		: scroll.get("x2", currentStep.get("x2", screenWidth / 2)).asInt();
	int y2 = scroll.isMember("y2Ratio")
		? (int)(scroll["y2Ratio"].asDouble() * screenHeight)
		: scroll.get("y2", currentStep.get("y2", screenHeight / 4)).asInt();

	int jitterX = scroll.get("jitterX", 18).asInt();
	int jitterY = scroll.get("jitterY", 18).asInt();
	int durationJitter = scroll.get("durationJitter", 120).asInt();
	if (jitterX > 0)
	{
		x1 += CommonMath::random(-jitterX, jitterX);
		x2 += CommonMath::random(-jitterX, jitterX);
	}
	if (jitterY > 0)
	{
		y1 += CommonMath::random(-jitterY, jitterY);
		y2 += CommonMath::random(-jitterY, jitterY);
	}
	if (durationJitter > 0)
	{
		duration += CommonMath::random(-durationJitter, durationJitter);
		if (duration < 120) duration = 120;
	}

	if (x1 < 0) x1 = 0; if (x1 >= screenWidth) x1 = screenWidth - 1;
	if (x2 < 0) x2 = 0; if (x2 >= screenWidth) x2 = screenWidth - 1;
	if (y1 < 0) y1 = 0; if (y1 >= screenHeight) y1 = screenHeight - 1;
	if (y2 < 0) y2 = 0; if (y2 >= screenHeight) y2 = screenHeight - 1;

	std::cout << "swipe from (" << x1 << "," << y1 << ") to (" << x2 << "," << y2 << ") duration=" << duration << std::endl;
	controller->swipe(x1, y1, x2, y2, duration);
	return true;
}

bool TaskRunner::ensureOcrRunning()
{
	if (ocr->isRunning())
	{
		return true;
	}
	return ocr->start();
}

void TaskRunner::sleepByStepDelay(Json::Value& currentStep, const std::string& msName, const std::string& secondName, int defaultSeconds)
{
	int ms = currentStep.get(msName, -1).asInt();
	if (ms < 0)
	{
		ms = currentStep.get(secondName, defaultSeconds).asInt() * 1000;
	}
	if (ms > 0)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(ms));
	}
}

void TaskRunner::sleepBeforeClick(Json::Value& currentStep, int legacyDelaySeconds)
{
	int defaultSeconds = currentStep.isMember("beforeDelay") || currentStep.isMember("beforeDelayMs") ? 0 : legacyDelaySeconds;
	sleepByStepDelay(currentStep, "beforeDelayMs", "beforeDelay", defaultSeconds);
}

void TaskRunner::sleepAfterClick(Json::Value& currentStep)
{
	sleepByStepDelay(currentStep, "afterDelayMs", "afterDelay", 0);
}
void TaskRunner::handleOcrAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime)
{
	cv::Point clickPoint;
	bool found = waitResolveClickPoint(currentStep, "ocr_match", clickPoint);

	if (found && currentStep.get("click", false).asBool())
	{
		sleepBeforeClick(currentStep, sleepTime);
		controller->click(clickPoint.x, clickPoint.y);
		sleepAfterClick(currentStep);

		cv::Mat afterClickImg = controller->captureScreen();
		saveCaptureImage(afterClickImg, "ocr_done");
	}

	if (currentStep.get("target", currentStep.get("text", "")).asString().empty() || found)
	{
		i = 0;
		nextStep = currentStep["next"];
	}
	else
	{
		i++;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

bool TaskRunner::collectTemplateMatchPoints(Json::Value& candidate, const cv::Mat& captureImg, std::vector<cv::Point>& points)
{
	std::string templateImgPath = candidate["template"].asString();
	cv::Mat templateImg = cv::imread(templateImgPath);
	if (templateImg.empty())
	{
		std::cerr << "failed to load template image: " << templateImgPath << std::endl;
		return false;
	}
	if (captureImg.cols < templateImg.cols || captureImg.rows < templateImg.rows)
	{
		return false;
	}

	double threshold = candidate.get("threshold", 0.89).asDouble();
	int matchType = candidate.get("matchType", (int)cv::TM_CCORR_NORMED).asInt();
	int minDistance = candidate.get("minDistance", 24).asInt();
	cv::Mat result;
	cv::matchTemplate(captureImg, templateImg, result, matchType);

	bool lowerIsBetter = matchType == cv::TM_SQDIFF || matchType == cv::TM_SQDIFF_NORMED;
	for (int y = 0; y < result.rows; ++y)
	{
		for (int x = 0; x < result.cols; ++x)
		{
			float score = result.at<float>(y, x);
			bool matched = lowerIsBetter ? score <= threshold : score >= threshold;
			if (!matched)
			{
				continue;
			}

			cv::Point center(x + templateImg.cols / 2, y + templateImg.rows / 2);
			bool duplicate = false;
			for (const auto& existing : points)
			{
				if (std::abs(existing.x - center.x) <= minDistance && std::abs(existing.y - center.y) <= minDistance)
				{
					duplicate = true;
					break;
				}
			}
			if (!duplicate)
			{
				points.push_back(center);
			}
		}
	}
	return !points.empty();
}

int TaskRunner::collectPriorityPoints(Json::Value& currentStep, int scrollIndex, std::vector<PriorityRecord>& records, int maxPriorityExclusive)
{
	if (!currentStep.isMember("candidates") || !currentStep["candidates"].isArray())
	{
		std::cerr << "collectPrioritySearch requires candidates array" << std::endl;
		return -1;
	}

	cv::Mat captureImg = controller->captureScreen();
	if (captureImg.empty())
	{
		std::cerr << "capture image is empty" << std::endl;
		return -1;
	}

	cv::Point roiOffset(0, 0);
	cv::Mat recogImg = applyRecognitionRoi(currentStep, captureImg, roiOffset);
	if (recogImg.empty())
	{
		return -1;
	}

	int beforeCount = (int)records.size();
	int bestMatchedPriority = -1;
	std::string group = currentStep.get("group", "default").asString();
	std::string region = currentStep.get("region", "").asString();
	int candidateCount = (int)currentStep["candidates"].size();
	if (maxPriorityExclusive < 0 || maxPriorityExclusive > candidateCount)
	{
		maxPriorityExclusive = candidateCount;
	}

	for (Json::ArrayIndex idx = 0; idx < (Json::ArrayIndex)maxPriorityExclusive; ++idx)
	{
		Json::Value candidate = currentStep["candidates"][idx];
		if (!candidate.isMember("template"))
		{
			continue;
		}
		if (!candidate.isMember("threshold") && currentStep.isMember("threshold"))
		{
			candidate["threshold"] = currentStep["threshold"];
		}
		if (!candidate.isMember("matchType") && currentStep.isMember("matchType"))
		{
			candidate["matchType"] = currentStep["matchType"];
		}

		std::vector<cv::Point> points;
		if (!collectTemplateMatchPoints(candidate, recogImg, points))
		{
			continue;
		}

		bestMatchedPriority = (int)idx;
		for (const auto& point : points)
		{
			PriorityRecord record;
			record.group = group;
			record.region = region;
			record.name = candidate.get("name", candidate["template"].asString()).asString();
			record.templatePath = candidate["template"].asString();
			record.priority = (int)idx;
			record.scrollIndex = scrollIndex;
			record.point = cv::Point(point.x + roiOffset.x, point.y + roiOffset.y);
			records.push_back(record);
			std::cout << "record card: region=" << record.region << " name=" << record.name
				<< " priority=" << record.priority << " scroll=" << record.scrollIndex
				<< " point=(" << record.point.x << "," << record.point.y << ")" << std::endl;
		}

		if (currentStep.get("stopLowerPriorityOnMatch", true).asBool())
		{
			std::cout << "matched priority " << idx << ", skip same/lower priority candidates" << std::endl;
			break;
		}
	}

	if ((int)records.size() > beforeCount)
	{
		saveCaptureImage(recogImg, "priority_collect");
	}
	return bestMatchedPriority;
}
bool TaskRunner::clickRegionSelector(Json::Value& currentStep, const std::string& region)
{
	std::string currentRegion = currentStep.get("currentRegion", "").asString();
	if (region.empty() || region == currentRegion)
	{
		return true;
	}
	if (!currentStep.isMember("selectors") || !currentStep["selectors"].isMember(region))
	{
		std::cerr << "missing selector for region: " << region << std::endl;
		return false;
	}

	Json::Value selector = currentStep["selectors"][region];
	cv::Point clickPoint;
	if (!waitResolveClickPoint(selector, "region_selector", clickPoint))
	{
		return false;
	}
	controller->click(clickPoint.x, clickPoint.y);
	sleepAfterClick(selector);
	return true;
}

bool TaskRunner::resetScrollPosition(Json::Value& currentStep)
{
	int count = currentStep.get("resetScrolls", currentStep.get("maxScrolls", 0).asInt() + 2).asInt();
	if (count <= 0)
	{
		return true;
	}

	Json::Value originalScroll = currentStep["scroll"];
	Json::Value resetStep = currentStep;
	Json::Value resetScroll = currentStep.isMember("resetScroll") ? currentStep["resetScroll"] : originalScroll;
	if (!currentStep.isMember("resetScroll"))
	{
		if (originalScroll.isMember("x1Ratio") || originalScroll.isMember("y1Ratio")
			|| originalScroll.isMember("x2Ratio") || originalScroll.isMember("y2Ratio"))
		{
			resetScroll["x1Ratio"] = originalScroll.get("x2Ratio", originalScroll.get("x1Ratio", 0.5));
			resetScroll["y1Ratio"] = originalScroll.get("y2Ratio", originalScroll.get("y1Ratio", 0.25));
			resetScroll["x2Ratio"] = originalScroll.get("x1Ratio", originalScroll.get("x2Ratio", 0.5));
			resetScroll["y2Ratio"] = originalScroll.get("y1Ratio", originalScroll.get("y2Ratio", 0.75));
		}
		else
		{
			resetScroll["deltaX"] = -originalScroll.get("deltaX", 0).asInt();
			resetScroll["deltaY"] = -originalScroll.get("deltaY", -300).asInt();
		}
	}
	resetStep["scroll"] = resetScroll;

	for (int idx = 0; idx < count; ++idx)
	{
		if (!performConfiguredScroll(resetStep))
		{
			return false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(currentStep.get("recognitionIntervalMs", 500).asInt()));
	}
	return true;
}

bool TaskRunner::replayScrollToIndex(Json::Value& currentStep, int scrollIndex)
{
	if (!resetScrollPosition(currentStep))
	{
		return false;
	}
	for (int idx = 0; idx < scrollIndex; ++idx)
	{
		if (!performConfiguredScroll(currentStep))
		{
			return false;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(currentStep.get("recognitionIntervalMs", 500).asInt()));
	}
	return true;
}
void TaskRunner::handlePrioritySearchAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime)
{
	sleepBeforeClick(currentStep, sleepTime);

	int maxScrolls = currentStep.get("maxScrolls", 0).asInt();
	int intervalMs = currentStep.get("recognitionIntervalMs", 500).asInt();
	if (intervalMs < 50) intervalMs = 50;
	int timeoutMs = currentStep.get("recognitionTimeoutMs", currentStep.get("timeoutMs", -1)).asInt();
	if (timeoutMs < 0)
	{
		timeoutMs = currentStep.get("recognitionTimeout", currentStep.get("timeout", 0)).asInt() * 1000;
	}
	auto start = std::chrono::steady_clock::now();

	for (int attempt = 0; attempt <= maxScrolls; ++attempt)
	{
		if (timeoutMs > 0)
		{
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
			if (elapsed >= timeoutMs)
			{
				std::cerr << "prioritySearch timeout after " << elapsed << "ms" << std::endl;
				break;
			}
		}
		cv::Point clickPoint;
		std::string matchedName;
		if (resolvePriorityPoint(currentStep, clickPoint, matchedName))
		{
			controller->click(clickPoint.x, clickPoint.y);
			sleepAfterClick(currentStep);

			cv::Mat afterClickImg = controller->captureScreen();
			saveCaptureImage(afterClickImg, "priority_done");

			i = 0;
			nextStep = currentStep["next"];
			return;
		}

		if (attempt < maxScrolls)
		{
			if (!performConfiguredScroll(currentStep))
			{
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
		}
	}

	i++;
	std::this_thread::sleep_for(std::chrono::milliseconds(500));
}

void TaskRunner::handleCollectPrioritySearchAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime)
{
	sleepBeforeClick(currentStep, sleepTime);

	std::string group = currentStep.get("group", "default").asString();
	if (currentStep.get("clearExisting", false).asBool())
	{
		m_priorityRecords.erase(
			std::remove_if(m_priorityRecords.begin(), m_priorityRecords.end(), [&](const PriorityRecord& record) {
				return record.group == group;
			}),
			m_priorityRecords.end());
	}

	int maxScrolls = currentStep.get("maxScrolls", 0).asInt();
	int intervalMs = currentStep.get("recognitionIntervalMs", 500).asInt();
	if (intervalMs < 50) intervalMs = 50;
	int timeoutMs = currentStep.get("recognitionTimeoutMs", currentStep.get("timeoutMs", -1)).asInt();
	if (timeoutMs < 0)
	{
		timeoutMs = currentStep.get("recognitionTimeout", currentStep.get("timeout", 0)).asInt() * 1000;
	}
	auto start = std::chrono::steady_clock::now();
	int bestPriorityLimit = currentStep.isMember("candidates") ? (int)currentStep["candidates"].size() : 0;
	for (const auto& record : m_priorityRecords)
	{
		if (record.group == group && record.priority < bestPriorityLimit)
		{
			bestPriorityLimit = record.priority;
		}
	}

	for (int scrollIndex = 0; scrollIndex <= maxScrolls; ++scrollIndex)
	{
		m_visibleRegion = currentStep.get("region", "").asString();
		m_visibleScrollIndex = scrollIndex;
		if (timeoutMs > 0)
		{
			auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
			if (elapsed >= timeoutMs)
			{
				std::cerr << "collectPrioritySearch timeout after " << elapsed << "ms" << std::endl;
				break;
			}
		}

		if (bestPriorityLimit <= 0)
		{
			std::cout << "highest priority already found, stop collecting" << std::endl;
			break;
		}

		int beforeCollectCount = (int)m_priorityRecords.size();
		int matchedPriority = bestPriorityLimit > 0
			? collectPriorityPoints(currentStep, scrollIndex, m_priorityRecords, bestPriorityLimit)
			: -1;
		if (matchedPriority >= 0 && matchedPriority < bestPriorityLimit)
		{
			bestPriorityLimit = matchedPriority;
		}
		int afterCollectCount = (int)m_priorityRecords.size();
		if (bestPriorityLimit <= 0)
		{
			std::cout << "highest priority matched, stop collecting" << std::endl;
			break;
		}
		if (currentStep.get("stopWhenNoNewMatchesAfterFirst", false).asBool()
			&& afterCollectCount == beforeCollectCount)
		{
			std::cout << "no card matched on current page, stop collecting before swipe" << std::endl;
			break;
		}

		if (scrollIndex < maxScrolls)
		{
			bool stopOnStable = currentStep.get("stopOnStable", true).asBool();
			double stableDiffThreshold = currentStep.get("stableDiffThreshold", 1.0).asDouble();
			cv::Mat beforeScroll;
			if (stopOnStable)
			{
				beforeScroll = controller->captureScreen();
			}

			if (!performConfiguredScroll(currentStep))
			{
				break;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));

			if (stopOnStable && !beforeScroll.empty())
			{
				cv::Mat afterScroll = controller->captureScreen();
				if (!afterScroll.empty() && beforeScroll.size() == afterScroll.size())
				{
					cv::Mat diff;
					cv::absdiff(beforeScroll, afterScroll, diff);
					cv::Scalar meanDiff = cv::mean(diff);
					double avgDiff = (meanDiff[0] + meanDiff[1] + meanDiff[2]) / 3.0;
					if (avgDiff <= stableDiffThreshold)
					{
						std::cout << "scroll reached bottom, avgDiff=" << avgDiff << std::endl;
						break;
					}
				}
			}
		}
	}

	if (currentStep.get("resetAfterScan", true).asBool())
	{
		resetScrollPosition(currentStep);
	}

	if (bestPriorityLimit <= 0 && currentStep.isMember("nextOnHighestPriority"))
	{
		i = 0;
		nextStep = currentStep["nextOnHighestPriority"];
		return;
	}

	bool found = false;
	for (const auto& record : m_priorityRecords)
	{
		if (record.group == group)
		{
			found = true;
			break;
		}
	}
	if (!found && currentStep.get("requireMatch", false).asBool())
	{
		i++;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		return;
	}

	i = 0;
	nextStep = currentStep["next"];
}

void TaskRunner::handleSelectPriorityRecordAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime)
{
	sleepBeforeClick(currentStep, sleepTime);

	std::string group = currentStep.get("group", "default").asString();
	const PriorityRecord* best = NULL;
	for (const auto& record : m_priorityRecords)
	{
		if (record.group != group)
		{
			continue;
		}
		if (best == NULL || record.priority < best->priority)
		{
			best = &record;
		}
	}

	if (best == NULL)
	{
		std::cerr << "no priority record found for group: " << group << std::endl;
		i++;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		return;
	}

	std::cout << "select card: region=" << best->region << " name=" << best->name
		<< " priority=" << best->priority << " scroll=" << best->scrollIndex
		<< " point=(" << best->point.x << "," << best->point.y << ")" << std::endl;

	bool alreadyVisible = best->region == m_visibleRegion && best->scrollIndex == m_visibleScrollIndex;
	if (!alreadyVisible)
	{
		if (!clickRegionSelector(currentStep, best->region))
		{
			i++;
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			return;
		}
		m_visibleRegion = best->region;
		m_visibleScrollIndex = 0;
		if (!replayScrollToIndex(currentStep, best->scrollIndex))
		{
			i++;
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			return;
		}
		m_visibleScrollIndex = best->scrollIndex;
	}
	else
	{
		std::cout << "best card already visible, click directly" << std::endl;
	}

	controller->click(best->point.x, best->point.y);
	sleepAfterClick(currentStep);
	cv::Mat afterClickImg = controller->captureScreen();
	saveCaptureImage(afterClickImg, "priority_selected");

	i = 0;
	nextStep = currentStep["next"];
}
void TaskRunner::handleDoubleClickAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime)
{
	cv::Point clickPoint;
	if (!waitResolveClickPoint(currentStep, "doubleClick_match", clickPoint))
	{
		i++;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		return;
	}

	sleepBeforeClick(currentStep, sleepTime);
	controller->doubleClick(clickPoint.x, clickPoint.y);
	sleepAfterClick(currentStep);

	cv::Mat afterClickImg = controller->captureScreen();
	saveCaptureImage(afterClickImg, "doubleClick_done");

	i = 0;
	nextStep = currentStep["next"];
}
void TaskRunner::handleLongClickAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime)
{
	cv::Point clickPoint;
	if (!waitResolveClickPoint(currentStep, "longClick_match", clickPoint))
	{
		i++;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		return;
	}

	int duration = currentStep.get("duration", 1000).asInt();
	sleepBeforeClick(currentStep, sleepTime);
	controller->longClick(clickPoint.x, clickPoint.y, duration);
	sleepAfterClick(currentStep);

	cv::Mat afterClickImg = controller->captureScreen();
	saveCaptureImage(afterClickImg, "longClick_done");

	i = 0;
	nextStep = currentStep["next"];
}
void TaskRunner::handleSwipeAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime)
{
	int x1 = currentStep.get("x1", 0).asInt();
	int y1 = currentStep.get("y1", 0).asInt();
	int x2 = currentStep.get("x2", 0).asInt();
	int y2 = currentStep.get("y2", 0).asInt();
	int duration = currentStep.get("duration", 300).asInt();

	cv::Mat beforeImg = controller->captureScreen();
	saveCaptureImage(beforeImg, "swipe_before");

	if (sleepTime > 0)
	{
		std::this_thread::sleep_for(std::chrono::seconds(sleepTime));
	}
	controller->swipe(x1, y1, x2, y2, duration);

	cv::Mat afterImg = controller->captureScreen();
	saveCaptureImage(afterImg, "swipe_done");

	i = 0;
	nextStep = currentStep["next"];
}

Json::Value TaskRunner::parseJsonFromString(const std::string& jsonString) {
	Json::Reader reader;
	Json::Value root;

	if (!reader.parse(jsonString, root)) {
		std::cerr << "JSON parsing error: " << reader.getFormattedErrorMessages() << std::endl;
	}

	return root;
}

Json::Value TaskRunner::readTaskJson(std::string filePath)
{
	std::ifstream file(filePath);
	if (!file.is_open()) {
		std::cerr << "Failed to open the file." << std::endl;
		return NULL;
	}

	std::string jsonData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	Json::Value root = parseJsonFromString(jsonData);

	if (root.isNull()) {
		std::cerr << "JSON parsing error." << std::endl;
		return NULL;
	}
	return root;
}
