#include "WindowConfig.h"

WindowConfig::WindowConfig()
{
	this->windowName = "雷电模拟器";
	this->isSub = true;

}
Json::Value WindowConfig::parseJsonFromString(const std::string& jsonString) {
    Json::Reader reader;
    Json::Value root;

    if (!reader.parse(jsonString, root)) {
        std::cerr << "JSON parsing error: " << reader.getFormattedErrorMessages() << std::endl;
    }

    return root;
}
Json::Value WindowConfig::readTaskJson(std::string filePath)
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
WindowConfig::WindowConfig(std::string filepath)
{
    Json::Value config = readTaskJson(filepath);
    this->windowName = config["windowName"].asString().c_str();
    this->isSub = config["issub"].asBool();
}
