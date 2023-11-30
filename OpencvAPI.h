#pragma once
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
class OpencvAPI
{
	OpencvAPI();
	std::vector<cv::Point> TemplateMatch(cv::Mat screenImg, cv::Mat templatImg, int threshold, int type);
};

