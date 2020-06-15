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
    auto db_agents = db_.retrieve_agents();
    for (auto& ag : db_agents)
    {
        emit agent_changed(*ag);
        agents_[ag->id()] = std::move(ag);
    }
}

void model::file_changed(const parcel& pcl)
{

}

void model::heartbeat(const parcel &pcl)
{
    agent ag(pcl);
    std::unique_lock<std::mutex> lock(agent_guard_);
    auto found = agents_.find(ag.id());
    if (found != agents_.end())
    {
        if (*found->second != ag)
        {
            *found->second = ag;
            db_.update(*found->second);
            emit agent_changed(*found->second);
        }
    }
    else
    {
        found = agents_.insert(std::make_pair(ag.id(), std::make_unique<agent>(ag))).first;
        db_.store(*found->second);
        emit agent_changed(*found->second);
    }
}

}

}
