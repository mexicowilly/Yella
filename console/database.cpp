#include "postgres_db.hpp"

namespace yella
{

namespace console
{

database::database(const configuration& cnf)
    : config_(cnf)
{
}

std::unique_ptr<database> database::create(const configuration& cnf)
{
    if (cnf.db_type() == "postgres")
        return std::make_unique<postgres_db>(cnf);
    throw std::invalid_argument("Unrecognized database type: " + cnf.db_type());
}

}

}
