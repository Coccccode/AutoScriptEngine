#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <windows.h>
#include "WindowCapture.h"
#include "WindowConfig.h"
#include <json/json.h>
using namespace cv;
using namespace std;

int main()
{
	WindowConfig config;
	WindowCapture* capture = new WindowCapture(config);
	cv::Mat img = capture->capture();
	capture->saveImg(img);
	return 0;
}

