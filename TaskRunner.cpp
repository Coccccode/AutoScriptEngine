#include "TaskRunner.h"

TaskRunner::TaskRunner()
{
    api = new OpencvAPI();
    capture = new WindowCapture(config);
    mouse = new MouseController();
}

void TaskRunner::start()
{
    while(1)
    {
        if (taskList.empty())
        {
            continue;
        }
        std::string taskName = taskList.front();
        std::string filepath = taskName + ".json";
        Json::Value root;
        Json::StyledWriter writer;
        std::ofstream os;
        os.open(filepath);
        os << writer.write(root);
        os.close();
    }
}

void TaskRunner::popTask(std::string taskName)
{

}

void TaskRunner::runTask(std::string taskName)
{
    if (capture->getHwnd() == NULL)
    {
        std::cout << "未找到窗口" << std::endl;
        return;
    }
    std::string filepath = taskName + ".json";
    Json::Value root = readTaskJson(filepath);
    Json::StyledWriter writer;
    Json::Value nextStep = root[taskName]["next"];
    Json::Value currentStep;
    HWND hwnd = capture->getHwnd();
    int i = 0;
    while(1)
    {
        if (nextStep.size() == 0)
        {
            std::cout << "任务完成" << std::endl;
            break;
        }
        currentStep = root[nextStep[i].asString()];
        std::string action = currentStep["action"].asString();
        bool isMatch = false;
        if (action == "click")
        {
            std::string templateImgPath = currentStep["template"].asString();
            cv::Mat templateImg = cv::imread(templateImgPath);
            cv::Mat captureImg = capture->capture();
            cv::Point click_point = api->TemplateMatch(captureImg, templateImg, 0.99, 1, isMatch);
            if (!isMatch)
            {
                i++;
                i = i % nextStep.size();
                continue;
            }
            mouse->click_point_random(click_point, hwnd);
            i = 0;
            nextStep = currentStep["next"];
        }
        
    }
}
Json::Value TaskRunner::parseJsonFromString(const std::string& jsonString) {
    Json::Reader reader;
    Json::Value root;

    if (!reader.parse(jsonString, root)) {
        std::cerr << "JSON parsing error: " << reader.getFormattedErrorMessages() << std::endl;
    }

    return root;
}
Json::Value TaskRunner::readTaskJson(std::string filePath)
{
    // 读取 JSON 数据文件
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "Failed to open the file." << std::endl;
        return NULL; // 返回表示失败
    }

    // 读取文件内容到字符串
    std::string jsonData((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // 解析 JSON 字符串
    Json::Value root = parseJsonFromString(jsonData);

    // 检查是否解析成功
    if (root.isNull()) {
        std::cerr << "JSON parsing error." << std::endl;
        return NULL; // 返回表示失败
    }
    return root;
}

TaskRunner::~TaskRunner()
{
    delete api;
    delete capture;
}
