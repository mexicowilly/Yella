#include "test_impl.hpp"
#include <chucho/log.hpp>

namespace yella
{

namespace test
{

test_impl::test_impl(const YAML::Node& doc, const std::filesystem::path& plugin)
    : lmrk_("===> "),
      doc_(doc),
      working_dir_(std::filesystem::temp_directory_path() / "fake-agent" / "test-data")
{
    std::error_code ec;
    std::filesystem::remove_all(working_dir_, ec);
    std::filesystem::create_directories(working_dir_);
    CHUCHO_INFO_L_M(lmrk_, "Working directory: " << working_dir_);
    agent_ = std::make_unique<agent>(plugin, std::bind(&test_impl::receive_plugin_message, this, std::placeholders::_1), working_dir_.parent_path() / "agent-data");
}

test_impl::~test_impl()
{
    std::error_code ec;
    std::filesystem::remove_all(working_dir_, ec);
}

void test_impl::receive_plugin_message(const yella_parcel& pcl)
{
    receive_plugin_message_impl(pcl);
}

}

}