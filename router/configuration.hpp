#ifndef YELLA_CONFIGURATION_HPP__
#define YELLA_CONFIGURATION_HPP__

#include <string>
#include <vector>

namespace yella
{

namespace router
{

class configuration
{
public:
    configuration(int argc, char* argv[]);

    const std::string& agent_face() const;
    std::uint16_t agent_port() const;
    const std::vector<std::string>& consumption_queues() const;
    const std::string& file_name() const;
    const std::string& mq_broker() const;
    const std::string& mq_face() const;
    std::size_t worker_threads() const;

private:
    void parse_command_line(int argc, char* argv[]);
    void parse_config_file();

    std::string file_name_;
    std::uint16_t agent_port_;
    std::string agent_face_;
    size_t worker_threads_;
    std::string mq_face_;
    std::string mq_broker_;
    std::vector<std::string> consumption_queues_;
};

inline const std::string& configuration::agent_face() const
{
    return agent_face_;
}

inline std::uint16_t configuration::agent_port() const
{
    return agent_port_;
}

inline const std::vector<std::string>& configuration::consumption_queues() const
{
    return consumption_queues_;
}

inline const std::string& configuration::file_name() const
{
    return file_name_;
}

inline const std::string& configuration::mq_broker() const
{
    return mq_broker_;
}

inline const std::string& configuration::mq_face() const
{
    return mq_face_;
}

inline size_t configuration::worker_threads() const
{
    return worker_threads_;
}

}

}

#endif
