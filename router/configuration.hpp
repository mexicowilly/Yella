#ifndef YELLA_CONFIGURATION_HPP__
#define YELLA_CONFIGURATION_HPP__

#include <string>

namespace yella
{

namespace router
{

class configuration
{
public:
    configuration(const std::string& file_name);

    const std::string& agent_face() const;
    std::uint16_t agent_port() const;
    const std::string& file_name() const;
    size_t worker_threads() const;

private:
    std::string file_name_;
    std::uint16_t agent_port_;
    std::string agent_face_;
    size_t worker_threads_;
};

inline const std::string& configuration::agent_face() const
{
    return agent_face_;
}

inline std::uint16_t configuration::agent_port() const
{
    return agent_port_;
}

inline const std::string& configuration::file_name() const
{
    return file_name_;
}

inline size_t configuration::worker_threads() const
{
    return worker_threads_;
}

}

}

#endif
