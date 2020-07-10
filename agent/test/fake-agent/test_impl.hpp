#ifndef YELLA_TEST_IMPL_HPP__
#define YELLA_TEST_IMPL_HPP__

#include "agent.hpp"
#include <chucho/loggable.hpp>
#include <yaml-cpp/yaml.h>
#include <filesystem>

namespace yella
{

namespace test
{

class test_impl : public chucho::loggable<test_impl>, public plugin_message_receiver
{
public:
    virtual void run() = 0;

protected:
    test_impl(const YAML::Node& doc, const std::filesystem::path& plugin);

    const YAML::Node& doc_;
    agent agent_;
};

}

}

#endif
