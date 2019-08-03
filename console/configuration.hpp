#ifndef YELLA_CONFIGURATION_HPP__
#define YELLA_CONFIGURATION_HPP__

#include <string>

namespace yella
{

namespace console
{

class configuration
{
public:
    configuration(int argc, char* argv[]);

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
};

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
