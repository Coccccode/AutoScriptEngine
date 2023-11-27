#include "TaskRunner.h"

TaskRunner::TaskRunner(std::string taskName)
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

}
