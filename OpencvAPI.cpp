#include "OpencvAPI.h"


std::vector<cv::Point> OpencvAPI::MultiTemplateMatch(cv::Mat screenImg,cv::Mat templateImg,double threshold, int type)
{
    std::vector<cv::Point> pointList;
	cv::Mat result;
    cv::Mat locations;
    int templateWidth = templateImg.cols;
    int templateHeight = templateImg.rows;
    // 在结果矩阵中找到匹配位置
    cv::Point minLoc, maxLoc;
    double minVal, maxVal;
    
    cv::matchTemplate(screenImg, templateImg,result, cv::TM_CCOEFF_NORMED);
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    // 绘制矩形框标记匹配位置
    while (maxVal >= threshold) {
        cv::rectangle(screenImg, maxLoc, cv::Point(maxLoc.x + templateImg.cols, maxLoc.y + templateImg.rows), cv::Scalar(0, 255, 0), 2);
        pointList.push_back(cv::Point(maxLoc.x + templateImg.cols / 2, maxLoc.y + templateImg.rows / 2));
        // 将当前找到的最大匹配置为阈值以下，继续寻找下一个
        result.at<float>(maxLoc.y, maxLoc.x) = 0;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    }
    // 显示结果
    cv::imshow("Matched Result", screenImg);
    cv::waitKey(0);
    cv::destroyAllWindows();
	return pointList;
}

cv::Point OpencvAPI::TemplateMatch(cv::Mat screenImg, cv::Mat templateImg, double threshold, int type)
{
    cv::Point point;
    cv::Mat result;
    cv::Mat locations;
    int templateWidth = templateImg.cols;
    int templateHeight = templateImg.rows;
    // 在结果矩阵中找到匹配位置
    cv::Point minLoc, maxLoc;
    double minVal, maxVal;

    cv::matchTemplate(screenImg, templateImg, result, cv::TM_CCOEFF_NORMED);
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    // 绘制矩形框标记匹配位置
    if (maxVal >= threshold) {
        cv::rectangle(screenImg, maxLoc, cv::Point(maxLoc.x + templateImg.cols, maxLoc.y + templateImg.rows), cv::Scalar(0, 255, 0), 2);
        point = cv::Point(maxLoc.x + templateImg.cols / 2, maxLoc.y + templateImg.rows / 2);
    }
    // 显示结果
    cv::imshow("Matched Result", screenImg);
    cv::waitKey(0);
    cv::destroyAllWindows();
    return point;
}

