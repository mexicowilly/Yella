#include "configuration.hpp"
#include <yaml-cpp/yaml.h>
#include <chucho/log.hpp>
#include <thread>

namespace yella
{

namespace router
{

configuration::configuration(const std::string& file_name)
    : file_name_(file_name),
      agent_port_(19567),
      agent_face_("zeromq"),
      worker_threads_(std::thread::hardware_concurrency())
{
    try
    {
        YAML::Node yaml = YAML::LoadFile(file_name);
        if (yaml["agent_port"])
            agent_port_ = yaml["agent_port"].as<std::uint16_t>();
        if (yaml["agent_face"])
            agent_face_ = yaml["agent_face"].as<std::string>();
        if (yaml["worker_threads"])
            worker_threads_ = yaml["worker_threads"].as<size_t>();
    }
    catch (const std::exception& e)
    {
        CHUCHO_ERROR(chucho::logger::get("yella.configuration"), "Unable to load configuration file '" << file_name << "': " << e.what());
    }
}

}

}