#include "OpencvAPI.h"


std::vector<cv::Point> OpencvAPI::TemplateMatch(cv::Mat screenImg,cv::Mat templateImg,int threshold, int type)
{
	cv::Mat result;
	cv::matchTemplate(screenImg, templateImg,result, type);
	return std::vector<cv::Point>();
}
