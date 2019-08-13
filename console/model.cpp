#include "model.hpp"

namespace yella
{

namespace console
{

model::model(const configuration& cnf, std::unique_ptr<database> db)
    : config_(cnf),
      db_(std::move(db))
{
}

void model::heartbeat_handler(const parcel &pcl)
{
    auto ag = agent(pcl);
    std::unique_lock<std::mutex> lock(agent_guard_);
    auto found = agents_.find(ag.id());
    bool present = found != agents_.end();
    bool equal = present && found->second == ag;
    if (!equal)
        agents_[ag.id()] = ag;
    lock.unlock();
    if (!present)
    {
        db_->store(ag);
    }
    else if (!equal)
    {
    }
}

}

}
