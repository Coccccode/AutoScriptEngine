#include "OcrApi.h"
#include <stdlib.h>
#include <stdio.h>
#include <windows.h>

OcrApi::OcrApi(int port)
{
    if (SetCurrentDirectory(L"PaddleOCR-json_v.1.3.1") == 0) {
        // 处理错误，例如路径无效
        printf("Error changing directory: %d\n", GetLastError());
    }
	system("PaddleOCR-json.exe -port=8888");
}
