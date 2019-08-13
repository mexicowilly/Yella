#ifndef YELLA_MODEL_HPP__
#define YELLA_MODEL_HPP__

#include "agent.hpp"
#include "database.hpp"
#include <map>
#include <mutex>

namespace yella
{

namespace console
{

class model
{
public:
    model(const configuration& cnf, std::unique_ptr<database> db);

    void heartbeat_handler(const parcel& pcl);

private:
    const configuration& config_;
    std::unique_ptr<database> db_;
    std::map<std::string, agent> agents_;
    std::mutex agent_guard_;
};

}

}

#endif
