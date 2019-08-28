#include "postgres_db.hpp"
#include <chucho/log.hpp>
#include <iomanip>
#include <ctime>

namespace yella
{

namespace console
{

postgres_db::postgres_db(const configuration& cnf)
    : database(cnf),
      cxn_(cnf.db())
{
    cxn_.prepare("store_agent",
                 "INSERT INTO agent (id, last, host, ip_addresses, os) VALUES ($1, $2, $3, $4, $5);");
    cxn_.prepare("store_in_cap",
                 "INSERT INTO in_capability (agent_id, name, version) VALUES ($1, $2, $3);");
    cxn_.prepare("store_out_cap",
                 "INSERT INTO out_capability (agent_id, name, version, configs) VALUES ($1, $2, $3, $4);");
    cxn_.prepare("update_agent",
                 "UPDATE agent SET last = $2, host = $3, ip_addresses = $4, os = $5 WHERE id = $1;");
    cxn_.prepare("delete_agent",
                 "DELETE FROM agent WHERE id = $1;");
    cxn_.prepare("delete_in_cap",
                 "DELETE FROM in_capability WHERE agent_id = $1;");
    cxn_.prepare("delete_out_cap",
                 "DELETE FROM out_capability WHERE agent_id = $1;");
    CHUCHO_INFO_L("Connected to database");
}

std::string postgres_db::configs_param(const agent::capability& out_cap)
{
    std::ostringstream stream;
    stream << '{';
    auto itor = out_cap.configurations().begin();
    for (std::size_t i = 0; i < out_cap.configurations().size(); i++)
    {
        stream << '"' << *itor++ << '"';
        if (i < out_cap.configurations().size() - 1)
            stream << ',';
    }
    stream << '}';
    return stream.str();
}

std::string postgres_db::ip_addresses_param(const agent& ag)
{
    std::ostringstream stream;
    if (!ag.ip_addresses().empty())
    {
        stream << '{';
        auto itor = ag.ip_addresses().begin();
        for (std::size_t i = 0; i < ag.ip_addresses().size(); i++)
        {
            stream << '"' << *itor++ << '"';
            if (i < ag.ip_addresses().size() - 1)
                stream << ',';
        }
        stream << '}';
    }
    return stream.str();
}

std::string postgres_db::os_param(const agent& ag)
{
    std::ostringstream stream;
    stream << "(\"" << ag.os().machine() << "\",\"" << ag.os().version() << "\",\"" << ag.os().system() << "\",\"" << ag.os().release() << "\")";
    return stream.str();
}

std::vector<std::unique_ptr<agent>> postgres_db::retrieve_agents()
{
}

void postgres_db::store(const agent& ag)
{
    auto timestamp = timestamp_param(ag);
    auto os = os_param(ag);
    auto addrs = ip_addresses_param(ag);
    pqxx::work txn(cxn_);
    txn.prepared("store_agent")
        (ag.id())
        (timestamp)
        (ag.host(), !ag.host().empty())
        (addrs, !addrs.empty())
        (os)
        .exec();
    for (const auto& in : ag.in_caps())
    {
        txn.prepared("store_in_cap")
            (ag.id())
            (in.name())
            (in.version())
            .exec();
    }
    for (const auto& out : ag.out_caps())
    {
        auto configs = configs_param(out);
        txn.prepared("store_out_cap")
            (ag.id())
            (out.name())
            (out.version())
            (configs, !configs.empty())
            .exec();
    }
    txn.commit();
}

std::string postgres_db::timestamp_param(const agent& ag)
{
    std::ostringstream stream;
    auto tm = std::chrono::system_clock::to_time_t(ag.when());
    stream << std::put_time(std::gmtime(&tm), "%Y-%m-%d %H:%M:%S UTC");
    return stream.str();
}

void postgres_db::update(const agent& ag)
{
    auto timestamp = timestamp_param(ag);
    auto os = os_param(ag);
    auto addrs = ip_addresses_param(ag);
    pqxx::work txn(cxn_);
    txn.prepared("update_agent")
        (ag.id())
        (timestamp)
        (ag.host(), !ag.host().empty())
        (addrs, !addrs.empty())
        (os)
        .exec();
    txn.prepared("delete_in_cap")(ag.id()).exec();
    txn.prepared("delete_out_cap")(ag.id()).exec();
    for (const auto& in : ag.in_caps())
    {
        txn.prepared("store_in_cap")
            (ag.id())
            (in.name())
            (in.version())
            .exec();
    }
    for (const auto& out : ag.out_caps())
    {
        auto configs = configs_param(out);
        txn.prepared("store_out_cap")
            (ag.id())
            (out.name())
            (out.version())
            (configs, !configs.empty())
            .exec();
    }
    txn.commit();
}

}

}
