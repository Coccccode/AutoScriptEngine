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
#include "TaskRunner.h"
using namespace cv;
using namespace std;

int main(int argc, char* argv[])
{
	TaskRunner runner;
	std::string taskName = argc > 1 ? argv[1] : "JieJieKa";
	runner.runTask(taskName);
	return 0;
}

