#include "configuration.hpp"
#include "cxxopts.hpp"
#include <yaml-cpp/yaml.h>
#include <chucho/log.hpp>
#include <thread>
#include <iostream>

namespace yella
{

namespace router
{

configuration::configuration(int argc, char* argv[])
    : agent_port_(19567),
      agent_face_("zeromq"),
      worker_threads_(std::thread::hardware_concurrency()),
      mq_face_("rabbitmq"),
      bind_exchanges_(false),
      consumption_queues_({"yella.agent.configuration"})
{
    parse_command_line(argc, argv);
}

void configuration::parse_command_line(int argc, char* argv[])
{
    cxxopts::Options opts("yella-router", "Feeds Yella agents into a message queue");
    opts.add_options()
        ("agent-face", "The type of interface to the agents (zeromq)", cxxopts::value<std::string>())
        ("agent-port", "The port to which agents should connect", cxxopts::value<std::uint16_t>())
        ("bind-exchanges", "Bind exchanges to queues")
        ("consumption-queues", "Message queues from which to consume (comma-delimited)", cxxopts::value<std::vector<std::string>>())
        ("config-file", "The configuration file", cxxopts::value<std::string>())
        ("h,help", "Display this helpful messasge")
        ("max-face-deaths", "Number of face deaths allowed", cxxopts::value<std::size_t>())
        ("mq-broker", "The broker URL", cxxopts::value<std::string>())
        ("mq-face", "Interface to the message queue (rabbitmq)", cxxopts::value<std::string>())
        ("worker-threads", "The number of worker threads", cxxopts::value<std::size_t>());
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
    }
    if (result["agent-port"].count())
        agent_port_ = result["agent-port"].as<std::uint16_t>();
    if (result["agent-face"].count())
        agent_face_ = result["agent-face"].as<std::string>();
    if (result["worker-threads"].count())
        worker_threads_ = result["worker-threads"].as<std::size_t>();
    if (result["mq-face"].count())
        mq_face_ = result["mq-face"].as<std::string>();
    if (result["mq-broker"].count())
        mq_broker_ = result["mq-broker"].as<std::string>();
    if (mq_broker_.empty())
        throw std::runtime_error("mq_broker must be set in the configuration");
    if (result["bind-exchanges"].count())
        bind_exchanges_ = result["bind-exchanges"].as<bool>();
    if (result["consumption-queues"].count())
        consumption_queues_ = result["consumption-queues"].as<std::vector<std::string>>();
}

void configuration::parse_config_file()
{
    try
    {
        YAML::Node yaml = YAML::LoadFile(file_name_);
        if (yaml["agent_port"])
            agent_port_ = yaml["agent_port"].as<std::uint16_t>();
        if (yaml["agent_face"])
            agent_face_ = yaml["agent_face"].as<std::string>();
        if (yaml["worker_threads"])
            worker_threads_ = yaml["worker_threads"].as<size_t>();
        if (yaml["mq_face"])
            mq_face_ = yaml["mq_face"].as<std::string>();
        if (yaml["mq_broker"])
            mq_broker_ = yaml["mq_broker"].as<std::string>();
        if (yaml["consumption_queues"])
        {
            auto qs = yaml["consumption_queues"];
            if (qs.IsSequence())
            {
                for (std::size_t i = 0; i < qs.size(); i++)
                    consumption_queues_.push_back(qs[i].as<std::string>());
            }
        }
    }
    catch (const std::exception& e)
    {
        CHUCHO_ERROR(chucho::logger::get("yella.configuration"), "Unable to load configuration file '" << file_name_ << "': " << e.what());
    }
}

}

}