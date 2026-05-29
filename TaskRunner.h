#pragma once
#include <string>
#include <json/json.h>
#include <fstream>
#include <queue>
#include <vector>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <windows.h>
#include "WindowCapture.h"
#include "WindowConfig.h"
#include "WindowController.h"
#include "AdbController.h"
#include "Controller.h"
#include <json/json.h>
#include "OpencvAPI.h"
#include "MouseController.h"
#include "OcrApi.h"

class TaskRunner
{
public:
// 在类成员变量区域新增：
	std::string m_instanceKey;

// 修改构造函数声明：
	TaskRunner();
	TaskRunner(Controller* externalController, const std::string& instanceKey = "default");
	~TaskRunner();

	std::queue<std::string> taskList;
	void addTask(std::string taskName);
	void start();
	void popTask(std::string taskName);
	void runTask(std::string taskName);

	Json::Value readTaskJson(std::string filePath);
	Json::Value parseJsonFromString(const std::string& jsonString);

	OpencvAPI* cvapi;
	WindowConfig* config;
	Controller* controller;
	MouseController* mouse;
	OcrApi* ocr;

private:
	struct PriorityRecord
	{
		std::string group;
		std::string region;
		std::string name;
		std::string templatePath;
		int priority;
		int scrollIndex;
		cv::Point point;
	};

	void handleClickAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime);
	void handleDetectAction(Json::Value& currentStep, Json::Value& nextStep, int& i);
	void handleOcrAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime);
	void handleDoubleClickAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime);
	void handleLongClickAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime);
	void handleSwipeAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime);
	void handlePrioritySearchAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime);
	void handleCollectPrioritySearchAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime);
	void handleSelectPriorityRecordAction(Json::Value& currentStep, Json::Value& nextStep, int& i, int& sleepTime);

	bool resolveClickPoint(Json::Value& currentStep, const std::string& imageName, cv::Point& clickPoint);
	bool waitResolveClickPoint(Json::Value& currentStep, const std::string& imageName, cv::Point& clickPoint);
	bool resolvePriorityPoint(Json::Value& currentStep, cv::Point& clickPoint, std::string& matchedName);
	int collectPriorityPoints(Json::Value& currentStep, int scrollIndex, std::vector<PriorityRecord>& records, int maxPriorityExclusive);
	bool collectTemplateMatchPoints(Json::Value& candidate, const cv::Mat& captureImg, std::vector<cv::Point>& points);
	bool clickRegionSelector(Json::Value& currentStep, const std::string& region);
	bool replayScrollToIndex(Json::Value& currentStep, int scrollIndex);
	bool resetScrollPosition(Json::Value& currentStep);
	bool performConfiguredScroll(Json::Value& currentStep);
	bool resolveTemplateMatchPoint(Json::Value& currentStep, const cv::Mat& captureImg, cv::Point& clickPoint);
	bool resolveOcrPoint(Json::Value& currentStep, const cv::Mat& captureImg, cv::Point& clickPoint);
	bool resolveDirectHintPoint(Json::Value& currentStep, cv::Point& clickPoint);
	cv::Mat applyRecognitionRoi(Json::Value& currentStep, const cv::Mat& image, cv::Point& offset);
	bool ensureOcrRunning();
	void sleepByStepDelay(Json::Value& currentStep, const std::string& msName, const std::string& secondName, int defaultSeconds = 0);
	void sleepBeforeClick(Json::Value& currentStep, int legacyDelaySeconds);
	void sleepAfterClick(Json::Value& currentStep);

	void saveCaptureImage(const cv::Mat& image, const std::string& stepName);
	int m_imageIndex;
	bool m_ownsController;
	std::vector<PriorityRecord> m_priorityRecords;
	std::string m_visibleRegion;
	int m_visibleScrollIndex;
};
