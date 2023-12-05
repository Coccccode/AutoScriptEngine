#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <windows.h>
#include "WindowCapture.h"
#include "WindowConfig.h"
#include <json/json.h>
#include "OpencvAPI.h"
using namespace cv;
using namespace std;

int main()
{
	WindowConfig config;
	WindowCapture* capture = new WindowCapture(config);
	cv::Mat templateImg = cv::imread("template2.png");
	cv::Mat img = capture->capture();
	capture->saveImg(img);
	OpencvAPI* api = new OpencvAPI();
	api->MultiTemplateMatch(img, templateImg, 0.9, 1);
	return 0;
}

