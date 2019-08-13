#include "agent.hpp"
#include "heartbeat_generated.h"

namespace yella
{

namespace console
{

agent::agent(const parcel& pcl)
{
    if (pcl.comp() != parcel::compression::NONE)
        throw std::runtime_error("The heartbeat should not be compressed");
    auto hb = fb::Getheartbeat(pcl.payload().data());
    when_ = std::chrono::system_clock::from_time_t(hb->seconds_since_epoch());
    if (hb->id() == nullptr)
        throw std::runtime_error("The heartbest does not have id");
    id_ = hb->id()->str();
    if (hb->host() == nullptr)
        throw std::runtime_error("The heartbest does not have host");
    host_ = hb->host()->str();
    if (hb->ip_addresses() == nullptr)
        throw std::runtime_error("The heartbest does not have IP addresses");
    for (auto str : *hb->ip_addresses())
        ip_addresses_.insert(str->str());
    if (hb->os() == nullptr)
        throw std::runtime_error("The heartbest does not have operation system");
    operating_system_ = operating_system(*hb->os());
    if (hb->in_capabilities() != nullptr)
    {
        for (auto in_cap : *hb->in_capabilities())
            in_caps_.insert(capability(*in_cap));
    }
    if (hb->out_capabilities() != nullptr)
    {
        for (auto out_cap : *hb->out_capabilities())
            out_caps_.insert(capability(*out_cap));
    }
}

bool agent::operator== (const agent& ag) const
{
    return id_ == ag.id_ &&
           host_ == ag.host_ &&
           ip_addresses_ == ag.ip_addresses_ &&
           operating_system_ == ag.operating_system_ &&
           in_caps_ == ag.in_caps_ &&
           out_caps_ == ag.out_caps_;
}

agent::capability::capability(const fb::capability& cap)
    : version_(cap.version())
{
    if (cap.name() == nullptr)
        throw std::runtime_error("The capability does not have a name");
    name_ = cap.name()->str();
    if (cap.configurations() != nullptr)
    {
        for (auto str : *cap.configurations())
            configurations_.insert(str->str());
    }
}

bool agent::capability::operator== (const capability& cap) const
{
    return name_ == cap.name_ &&
           version_ == cap.version_ &&
           configurations_ == cap.configurations_;
}
agent::operating_system::operating_system(const fb::operating_system& os)
{
    if (os.machine() == nullptr)
        throw std::runtime_error("The operation system does not have a name");
    machine_ = os.machine()->str();
    if (os.version() == nullptr)
        throw std::runtime_error("The operation system does not have a version");
    version_ = os.version()->str();
    if (os.system() == nullptr)
        throw std::runtime_error("The operation system does not have a system");
    system_ = os.system()->str();
    if (os.release() == nullptr)
        throw std::runtime_error("The operation release does not have a release");
    release_ = os.release()->str();
}

bool agent::operating_system::operator== (const operating_system& os) const
{
    return machine_ == os.machine_ &&
           version_ == os.version_ &&
           system_ == os.system_ &&
           release_ == os.release_;
}
}

}
