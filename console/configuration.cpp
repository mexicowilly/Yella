#include "configuration.hpp"
#include "../router/cxxopts.hpp"
#include <yaml-cpp/yaml.h>
#include <chucho/log.hpp>
#include <chucho/configuration.hpp>
#include <thread>

namespace yella
{

namespace console
{

configuration::configuration(int argc, char* argv[])
    : mq_type_("rabbitmq"),
      mq_threads_(std::thread::hardware_concurrency()),
      consumption_queues_({"yella.agent.heartbeat", "yella.agent.file.change"}),
      db_type_("postgres")
{
cxxopts::Options opts("yella-console", "Allows control of yella agents");
    opts.add_options()
        ("config-file", "The configuration file", cxxopts::value<std::string>())
        ("consumption-queues", "Message queues from which to consume (comma-delimited)", cxxopts::value<std::vector<std::string>>())
        ("db", "The database", cxxopts::value<std::string>())
        ("db-type", "The database type (postgres)", cxxopts::value<std::string>())
        ("h,help", "Display this helpful messasge")
        ("mq-broker", "The broker URL", cxxopts::value<std::string>())
        ("mq-threads", "The number of threads to service the message queue")
        ("mq-type", "The type of message queue (rabbitmq)", cxxopts::value<std::string>());
    auto result = opts.parse(argc, argv);
    if (result["help"].as<bool>())
    {
        std::cout << opts.help() << std::endl;
        exit(EXIT_SUCCESS);
    }
    if (result["config-file"].count())
    {
        file_name_ = result["config-file"].as<std::string>();
        parse_config_file();
        chucho::configuration::set_file_name(file_name_);
    }
    if (result["mq-broker"].count())
        mq_broker_ = result["mq-broker"].as<std::string>();
    if (mq_broker_.empty())
        throw std::runtime_error("mq_broker must be set in the configuration");
    if (result["mq-type"].count())
        mq_type_ = result["mq-type"].as<std::string>();
    if (result["mq-threads"].count())
        mq_threads_ = result["mq-threads"].as<std::size_t>();
    if (result["consumption-queues"].count())
        consumption_queues_ = result["consumption-queues"].as<std::vector<std::string>>();
    if (result["db"].count())
        db_ = result["db"].as<std::string>();
    if (db_.empty())
        throw std::runtime_error("db must be set in the configuration");
    if (result["db-type"].count())
        db_type_ = result["db-type"].as<std::string>();
}

void configuration::parse_config_file()
{
    try
    {
        YAML::Node yaml = YAML::LoadFile(file_name_);
        if (yaml["mq_broker"])
            mq_broker_ = yaml["mq_broker"].as<std::string>();
        if (yaml["mq_type"])
            mq_type_ = yaml["mq_type"].as<std::string>();
        if (yaml["mq_threads"])
            mq_threads_ = yaml["mq_threads"].as<std::size_t>();
        if (yaml["consumption_queues"])
        {
            auto qs = yaml["consumption_queues"];
            if (qs.IsSequence())
            {
                consumption_queues_.clear();
                for (std::size_t i = 0; i < qs.size(); i++)
                    consumption_queues_.push_back(qs[i].as<std::string>());
            }
        }
        if (yaml["db"])
            db_ = yaml["db"].as<std::string>();
        if (yaml["db_type"])
            db_type_ = yaml["db_type"].as<std::string>();
    }
    catch (const std::exception& e)
    {
        CHUCHO_ERROR(chucho::logger::get("yella.configuration"), "Unable to load configuration file '" << file_name_ << "': " << e.what());
    }

}

}

}