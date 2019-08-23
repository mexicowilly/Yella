#include "model.hpp"

namespace yella
{

namespace console
{

model::model(const configuration& cnf, database& db, main_window& win)
    : config_(cnf),
      db_(db)
{
    QObject::connect(this, SIGNAL(agent_changed(const agent&)),
                     &win, SLOT(agent_changed(const agent&)));
}

void model::file_changed(const parcel& pcl)
{

}

void model::heartbeat(const parcel &pcl)
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
        db_.store(ag);
    else if (!equal)
        db_.update(ag);
    if (!equal)
        emit agent_changed(ag);
}

}

}
