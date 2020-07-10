#include "file_test_impl.hpp"
#include <chucho/log.hpp>

namespace yella
{

namespace test
{

file_test_impl::file_test_impl(const YAML::Node& doc, const std::filesystem::path& plugin)
    : test_impl(doc, plugin)
{
    rename_logger(typeid(file_test_impl));
}

void file_test_impl::receive_plugin_message(const yella_parcel& pcl)
{
}

void file_test_impl::run()
{
    for (const auto& cur : doc_)
    {
        CHUCHO_INFO_L("Running: " << cur.first.as<std::string>());
    }
}

}

}