#ifndef YELLA_FATAL_ERROR_HPP__
#define YELLA_FATAL_ERROR_HPP__

#include <stdexcept>

namespace yella
{

namespace router
{

class fatal_error : public std::runtime_error
{
public:
    fatal_error(const std::string& msg);
};

inline fatal_error::fatal_error(const std::string& msg)
    : std::runtime_error(msg)
{
}

}

}

#endif
