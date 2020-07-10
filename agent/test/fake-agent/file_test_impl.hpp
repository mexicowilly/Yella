#ifndef YELLA_FILE_TEST_IMPL_HPP__
#define YELLA_FILE_TEST_IMPL_HPP__

#include "test_impl.hpp"

namespace yella
{

namespace test
{

class file_test_impl : public test_impl
{
public:
    file_test_impl(const YAML::Node& doc, const std::filesystem::path& plugin);

    virtual void receive_plugin_message(const yella_parcel& pcl) override;
    virtual void run() override;
};

}

}

#endif
