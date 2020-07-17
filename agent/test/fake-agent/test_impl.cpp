#include "test_impl.hpp"

namespace yella
{

namespace test
{

test_impl::test_impl(const YAML::Node& doc, const std::filesystem::path& plugin)
    : doc_(doc),
      agent_(plugin, *this),
      lmrk_("===> ")
{
}

}

}