#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <windows.h>
#include "WindowCapture.h"
#include "WindowConfig.h"
#include <json/json.h>
#include "OpencvAPI.h"
#include "MouseController.h"
using namespace cv;
using namespace std;

int main()
{
	WindowConfig config;
	WindowCapture* capture = new WindowCapture(config);
	std::vector<cv::String> list = { "end1.png","end2.png"};
	int i = 0;
	while (1)
	{
		i = i % 2;
		cv::Mat templateImg1 = cv::imread(list[i]);
		cv::Mat img = capture->capture();
		OpencvAPI* api = new OpencvAPI();
		bool isMatch;
		cv::Point point = api->TemplateMatch(img, templateImg1, 0.99, 1,isMatch);
		if (!isMatch)
		{
			continue;
		}
		HWND hwnd = capture->getHwnd();
		MouseController* mouse = new MouseController();
		mouse->click_area_random(cv::Point(1132, 179), cv::Point(1200, 600),hwnd);
		i++;
	}
	
	return 0;
}

