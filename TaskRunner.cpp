#include "TaskRunner.h"

TaskRunner::TaskRunner()
{
	std::string filepath = taskName + ".json";

	Json::Value root;

    Json::StyledWriter writer;
    std::ofstream os;
    os.open(filepath);
    os << writer.write(root);
    os.close();

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
