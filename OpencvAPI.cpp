#include "OpencvAPI.h"


std::vector<cv::Point> OpencvAPI::MultiTemplateMatch(cv::Mat screenImg,cv::Mat templateImg,double threshold, int type)
{
    std::vector<cv::Point> pointList;
	cv::Mat result;
    cv::Mat locations;
    int templateWidth = templateImg.cols;
    int templateHeight = templateImg.rows;
    // �ڽ���������ҵ�ƥ��λ��
    cv::Point minLoc, maxLoc;
    double minVal, maxVal;
    
    cv::matchTemplate(screenImg, templateImg, result, type);
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    // ���ƾ��ο���ƥ��λ��
    while (maxVal >= threshold) {
        cv::rectangle(screenImg, maxLoc, cv::Point(maxLoc.x + templateImg.cols, maxLoc.y + templateImg.rows), cv::Scalar(0, 255, 0), 2);
        pointList.push_back(cv::Point(maxLoc.x + templateImg.cols / 2, maxLoc.y + templateImg.rows / 2));
        // ����ǰ�ҵ������ƥ����Ϊ��ֵ���£�����Ѱ����һ��
        result.at<float>(maxLoc.y, maxLoc.x) = 0;
        cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    }
    // ��ʾ���
    cv::imshow("Matched Result", screenImg);
    cv::waitKey(0);
    cv::destroyAllWindows();
	return pointList;
}

cv::Point OpencvAPI::TemplateMatch(cv::Mat screenImg, cv::Mat templateImg, double threshold, int type, bool& isTrue)
{
    cv::Point point(0, 0);
    cv::Mat result;
    int templateWidth = templateImg.cols;
    int templateHeight = templateImg.rows;
    cv::Point minLoc, maxLoc;
    double minVal, maxVal;
    int screenWidth = screenImg.cols;
    int screenHeight = screenImg.rows;
    isTrue = false;

    double scale = screenImg.cols / 1280.0;
    int targetWidth = scale * templateWidth;
    int targetHeight = scale * templateHeight;
    if (screenWidth != 1280)
    {
        cv::InterpolationFlags flags;
        if (scale > 1.0)
        {
            flags = cv::INTER_LINEAR;
        }
        else
        {
            flags = cv::INTER_AREA;
        }
        cv::Mat tempImg;
        cv::resize(templateImg, tempImg, cv::Size(targetWidth, targetHeight), 0, 0, flags);
        templateImg = tempImg;
    }

    cv::matchTemplate(screenImg, templateImg, result, type);
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);

    if (maxVal >= threshold) {
        cv::rectangle(screenImg, maxLoc, cv::Point(maxLoc.x + templateImg.cols, maxLoc.y + templateImg.rows), cv::Scalar(0, 255, 0), 2);
        point = cv::Point(maxLoc.x + templateImg.cols / 2, maxLoc.y + templateImg.rows / 2);
        isTrue = true;
        std::cout << "ƥ��ɹ�" << std::endl;
    }
    return point;
}


