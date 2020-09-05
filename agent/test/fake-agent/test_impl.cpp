#include "test_impl.hpp"
#include <chucho/log.hpp>

namespace yella
{

namespace test
{

test_impl::test_impl(const YAML::Node& doc, const std::filesystem::path& plugin)
    : doc_(doc),
      lmrk_("===> "),
      working_dir_(std::filesystem::temp_directory_path() / "fake-agent" / "test-data"),
      agent_(plugin, *this, working_dir_.parent_path() / "agent-data")
{
    std::filesystem::create_directories(working_dir_);
    CHUCHO_INFO_L_M(lmrk_, "Working directory: " << working_dir_);
}

test_impl::~test_impl()
{
    std::error_code ec;
    std::filesystem::remove_all(working_dir_, ec);
}

}

}