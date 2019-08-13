#ifndef YELLA_AGENT_HPP__
#define YELLA_AGENT_HPP__

#include "parcel.hpp"
#include "heartbeat_generated.h"
#include <chucho/loggable.hpp>
#include <set>

namespace yella
{

namespace console
{

class agent : public chucho::loggable<agent>
{
public:
    class operating_system
    {
    public:
        operating_system() = default;
        operating_system(const fb::operating_system& os);

        bool operator== (const operating_system& os) const;

        const std::string& machine() const;
        const std::string& release() const;
        const std::string& system() const;
        const std::string& version() const;

    private:
        std::string machine_;
        std::string version_;
        std::string system_;
        std::string release_;
    };

    class capability
    {
    public:
        capability(const fb::capability& cap);

        bool operator== (const capability& cap) const;
        bool operator!= (const capability& cap) const;
        bool operator< (const capability& cap) const;

        const std::string& name() const;
        int version() const;
        const std::set<std::string>& configurations() const;

    private:
        std::string name_;
        int version_;
        std::set<std::string> configurations_;
    };

    agent() = default;
    agent(const parcel& pcl);
    agent(const agent&) = default;

    agent& operator= (const agent&) = default;
    bool operator== (const agent& ag) const;
    bool operator!= (const agent& ag) const;

    const std::string& host() const;
    const std::string& id() const;
    const std::set<std::string>& ip_addresses() const;
    const operating_system& os() const;
    const std::set<capability>& in_caps() const;
    const std::set<capability>& out_caps() const;
    const std::chrono::system_clock::time_point& when() const;

private:
    std::chrono::system_clock::time_point when_;
    std::string id_;
    std::string host_;
    std::set<std::string> ip_addresses_;
    operating_system operating_system_;
    std::set<capability> in_caps_;
    std::set<capability> out_caps_;
};

inline bool agent::operator!= (const agent& ag) const
{
    return !operator==(ag);
}

inline const std::string& agent::host() const
{
    return host_;
}

inline const std::string& agent::id() const
{
    return id_;
}

inline const std::set<std::string>& agent::ip_addresses() const
{
    return ip_addresses_;
}

inline const agent::operating_system& agent::os() const
{
    return operating_system_;
}

inline const std::set<agent::capability>& agent::in_caps() const
{
    return in_caps_;
}

inline const std::set<agent::capability>& agent::out_caps() const
{
    return out_caps_;
}

inline const std::chrono::system_clock::time_point& agent::when() const
{
    return when_;
}

inline bool agent::capability::operator!= (const capability& cap) const
{
    return !operator==(cap);
}

inline bool agent::capability::operator< (const capability& cap) const
{
    return name_ < cap.name_;
}

inline const std::string& agent::capability::name() const
{
    return name_;
}

inline int agent::capability::version() const
{
    return version_;
}

inline const std::set<std::string>& agent::capability::configurations() const
{
    return configurations_;
}

inline const std::string& agent::operating_system::machine() const
{
    return machine_;
}

inline const std::string& agent::operating_system::release() const
{
    return release_;
}

inline const std::string& agent::operating_system::system() const
{
    return system_;
}

inline const std::string& agent::operating_system::version() const
{
    return version_;
}

}

}

#endif
