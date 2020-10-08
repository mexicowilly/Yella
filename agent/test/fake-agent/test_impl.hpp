#ifndef YELLA_TEST_IMPL_HPP__
#define YELLA_TEST_IMPL_HPP__

#include "agent.hpp"
#include <chucho/loggable.hpp>
#include <chucho/marker.hpp>
#include <yaml-cpp/yaml.h>
#include <filesystem>

namespace yella
{

namespace test
{

class test_impl : public chucho::loggable<test_impl>
{
public:
    virtual ~test_impl();

    virtual void run() = 0;

protected:
    test_impl(const YAML::Node& doc, const std::filesystem::path& plugin);

    void destroy_agent();
    virtual void receive_plugin_message_impl(const yella_parcel& pcl) = 0;

    const YAML::Node& doc_;
    chucho::marker lmrk_;
    std::filesystem::path working_dir_;
    std::unique_ptr<agent> agent_;

private:
    void receive_plugin_message(const yella_parcel& pcl);
};

inline void test_impl::destroy_agent()
{
    agent_.reset();
}

}

}

#endif