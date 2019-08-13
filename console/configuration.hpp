#ifndef YELLA_CONFIGURATION_HPP__
#define YELLA_CONFIGURATION_HPP__

#include <string>
#include <vector>

namespace yella
{

namespace console
{

class configuration
{
public:
    configuration(int argc, char* argv[]);

    const std::vector<std::string>& consumption_queues() const;
    const std::string& db() const;
    const std::string& db_type() const;
    const std::string& file_name() const;
    const std::string& mq_broker() const;
    std::size_t mq_threads() const;
    const std::string& mq_type() const;

public:
    void parse_config_file();

    std::string file_name_;
    std::string mq_broker_;
    std::string mq_type_;
    std::size_t mq_threads_;
    std::vector<std::string> consumption_queues_;
    std::string db_type_;
    std::string db_;
};

inline const std::vector<std::string>& configuration::consumption_queues() const
{
    return consumption_queues_;
}

inline const std::string& configuration::db() const
{
    return db_;
}

inline const std::string& configuration::db_type() const
{
    return db_type_;
}

inline const std::string& configuration::file_name() const
{
    return file_name_;
}

inline const std::string& configuration::mq_broker() const
{
    return mq_broker_;
}

inline std::size_t configuration::mq_threads() const
{
    return mq_threads_;
}

inline const std::string& configuration::mq_type() const
{
    return mq_type_;
}

}

}

#endif
