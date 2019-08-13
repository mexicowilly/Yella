#ifndef YELLA_DATABASE_HPP__
#define YELLA_DATABASE_HPP__

#include "agent.hpp"
#include "configuration.hpp"

namespace yella
{

namespace console
{

class database : public chucho::loggable<database>
{
public:
    static std::unique_ptr<database> create(const configuration& cnf);

    database(const database&) = delete;

    database& operator= (const database&) = delete;

    virtual void store(const agent& ag) = 0;
    virtual void update(const agent& ag) = 0;

protected:
    database(const configuration& cnf);

    const configuration& config_;
};

}

}

#endif
