#ifndef YELLA_POSTGRES_DB_HPP__
#define YELLA_POSTGRES_DB_HPP__

#include "database.hpp"
#include <pqxx/pqxx>

namespace yella
{

namespace console
{

class postgres_db : public database
{
public:
    postgres_db(const configuration& cnf);

    std::vector<std::unique_ptr<agent>> retrieve_agents() override;
    void store(const agent& ag) override;
    void update(const agent& ag) override;

private:
    void store_ips_and_caps(pqxx::work& txn, const agent& ag);
    std::chrono::system_clock::time_point timestamp_field(const char* const str);
    std::string timestamp_param(const agent& ag);

    pqxx::connection cxn_;
};

}

}

#endif
