#pragma once
#include <string>
#include <vector>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <Windows.h>
#include <json/json.h>

#pragma comment(lib, "Ws2_32.lib")

struct OcrResult
{
	std::string text;
	double score;
	int x;
	int y;
	int width;
	int height;
};

class OcrApi
{
public:
	OcrApi(int port = 8888, const std::string& configName = "config_chinese.txt");
	~OcrApi();

	bool start();
	bool isRunning();
	std::vector<OcrResult> recognize(const cv::Mat& image);
	std::vector<OcrResult> recognize(const cv::Mat& image, const std::string& configName);
	std::string findText(const cv::Mat& image, const std::string& target);

private:
	int m_port;
	std::string m_configName;
	HANDLE m_processHandle;
	bool m_running;

	std::string matToBase64(const cv::Mat& image);
	std::string matToTempImage(const cv::Mat& image);
	std::string sendSocketRequest(const std::string& host, int port, const std::string& body);
	bool canConnect();
	std::string base64Encode(const unsigned char* data, size_t len);
	std::vector<OcrResult> parseOcrResponse(const std::string& jsonStr);
};
