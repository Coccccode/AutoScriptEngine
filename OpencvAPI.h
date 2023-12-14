#pragma once
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
class OpencvAPI
{

public:
	std::vector<cv::Point> MultiTemplateMatch(cv::Mat screenImg, cv::Mat templatImg, double threshold, int type);
	cv::Point TemplateMatch(cv::Mat screenImg, cv::Mat templateImg, double threshold, int type,bool& isTrue);

};

