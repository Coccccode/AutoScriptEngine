#pragma once
#include "Controller.h"
#include <stdio.h>
#include <tchar.h>
#include <iostream>
#include <string>
#include <cassert>
#include <Windows.h>
#include "CommonMath.h"
class MouseController :
    public Controller
{
    void click_point_random(unsigned long x, unsigned long y, HWND& hWnd);
    void click_point(unsigned long x, unsigned long y, HWND& hWnd);
};

