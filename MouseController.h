#pragma once
#include "Controller.h"
#include <stdio.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <cassert>
#include <Windows.h>
#include "CommonMath.h"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
class MouseController :
    public Controller
{
public:
    void click_point_random(unsigned long x, unsigned long y, HWND& hWnd);
    void click_point(unsigned long x, unsigned long y, HWND& hWnd);
    void click_area_random(cv::Point a, cv::Point b, HWND& hWnd);
    void long_click_point(unsigned long x, unsigned long y, HWND& hWnd);
};

