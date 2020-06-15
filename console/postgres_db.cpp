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
                 "INSERT INTO agent (id, last, host, machine, operating_system, os_version, os_release) VALUES ($1, $2, $3, $4, $5, $6, $7);");
    cxn_.prepare("store_ip_address",
                 "INSERT INTO ip_address (agent_id, address) VALUES ($1, $2);");
    cxn_.prepare("store_in_cap",
                 "INSERT INTO in_capability (agent_id, name, version) VALUES ($1, $2, $3);");
    cxn_.prepare("store_out_cap",
                 "INSERT INTO out_capability (id, agent_id, name, version) VALUES (DEFAULT, $1, $2, $3) RETURNING id;");
    cxn_.prepare("store_config",
                 "INSERT INTO configuration (out_cap_id, name) VALUES ($1, $2);");
    cxn_.prepare("update_agent",
                 "UPDATE agent SET last = $2, host = $3, machine = $4, operating_system = $5, os_version = $6, os_release = $7 WHERE id = $1;");
    cxn_.prepare("delete_agent",
                 "DELETE FROM agent WHERE id = $1;");
    cxn_.prepare("delete_in_cap",
                 "DELETE FROM in_capability WHERE agent_id = $1;");
    cxn_.prepare("delete_out_cap",
                 "DELETE FROM out_capability WHERE agent_id = $1;");
    cxn_.prepare("delete_ip_address",
                 "DELETE FROM ip_address WHERE agent_id = $1;");
    cxn_.prepare("delete_configuration",
                 "DELETE FROM configuration USING out_capability WHERE out_capability.agent_id = $1 AND out_capability.id = out_cap_id;");
    cxn_.prepare("retrieve_agents",
                 "SELECT * FROM agent;");
    cxn_.prepare("retrieve_in_caps",
                 "SELECT name, version FROM in_capability WHERE agent_id = $1;");
    cxn_.prepare("retrieve_out_caps",
                 "SELECT id, name, version FROM out_capability WHERE agent_id = $1;");
    cxn_.prepare("retrieve_ip_addresses",
                 "SELECT address FROM ip_address WHERE agent_id = $1;");
    cxn_.prepare("retrieve_configs",
                 "SELECT name FROM configuration WHERE out_cap_id = $1;");
    CHUCHO_INFO_L("Connected to database");
}

std::vector<std::unique_ptr<agent>> postgres_db::retrieve_agents()
{
    std::vector<std::unique_ptr<agent>> result;
    pqxx::work txn(cxn_);
    auto ags = txn.prepared("retrieve_agents").exec();
    for (const auto& ag : ags)
    {
        auto db_ips = txn.prepared("retrieve_ip_addresses")(ag[0].c_str()).exec();
        std::set<std::string> ips;
        for (const auto& ip : db_ips)
            ips.emplace(ip[0].c_str());
        auto db_in_caps = txn.prepared("retrieve_in_caps")(ag[0].c_str()).exec();
        std::set<agent::capability> in_caps;
        for (const auto& in_cap : db_in_caps)
            in_caps.emplace(in_cap[0].c_str(), in_cap[1].as<int>());
        auto db_out_caps = txn.prepared("retrieve_out_caps")(ag[0].c_str()).exec();
        std::set<agent::capability> out_caps;
        for (const auto& out_cap : db_out_caps)
        {
            auto db_configs = txn.prepared("retrieve_configs")(out_cap[0].as<int>()).exec();
            std::set<std::string> configs;
            for (const auto& config : db_configs)
                configs.emplace(config[0].c_str());
            out_caps.emplace(out_cap[1].c_str(), out_cap[2].as<int>(), configs);
        }
        CHUCHO_INFO_L("*** Timestamp: " << ag[1].c_str());
        result.emplace_back(std::make_unique<agent>(ag[0].c_str(),
                                                    timestamp_field(ag[1].c_str()),
                                                    ag[2].c_str(),
                                                    ips,
                                                    agent::operating_system(ag[3].c_str(),
                                                                            ag[4].c_str(),
                                                                            ag[5].c_str(),
                                                                            ag[6].c_str()),
                                                    in_caps,
                                                    out_caps));
    }
    return result;
}

void postgres_db::store(const agent& ag)
{
    auto timestamp = timestamp_param(ag);
    pqxx::work txn(cxn_);
    txn.prepared("store_agent")
        (ag.id())
        (timestamp)
        (ag.host(), !ag.host().empty())
        (ag.os().machine(), !ag.os().machine().empty())
        (ag.os().system(), !ag.os().system().empty())
        (ag.os().version(), !ag.os().version().empty())
        (ag.os().release(), !ag.os().release().empty())
        .exec();
    store_ips_and_caps(txn, ag);
    txn.commit();
}

void postgres_db::store_ips_and_caps(pqxx::work& txn, const agent& ag)
{
    for (const auto& addr : ag.ip_addresses())
        txn.prepared("store_ip_address")(ag.id())(addr).exec();
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
        auto res = txn.prepared("store_out_cap")
                (ag.id())
                (out.name())
                (out.version())
            .exec();
        for (const auto& config : out.configurations())
            txn.prepared("store_config")(res[0][0].as<int>())(config).exec();
    }
}

std::chrono::system_clock::time_point postgres_db::timestamp_field(const char* const str)
{
    std::tm pieces;
    std::istringstream stream(str);
    stream >> std::get_time(&pieces, "%Y-%m-%d %H:%M:%S+00");
    return std::chrono::system_clock::from_time_t(std::mktime(&pieces));
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
    pqxx::work txn(cxn_);
    txn.prepared("update_agent")
        (ag.id())
        (timestamp)
        (ag.host(), !ag.host().empty())
        (ag.os().machine(), !ag.os().machine().empty())
        (ag.os().system(), !ag.os().system().empty())
        (ag.os().version(), !ag.os().version().empty())
        (ag.os().release(), !ag.os().release().empty())
        .exec();
    txn.prepared("delete_ip_address")(ag.id()).exec();
    txn.prepared("delete_in_cap")(ag.id()).exec();
    txn.prepared("delete_configuration")(ag.id()).exec();
    txn.prepared("delete_out_cap")(ag.id()).exec();
    store_ips_and_caps(txn, ag);
    txn.commit();
}

}

}
