#include "test.hpp"
#include "file_test_impl.hpp"
#include <chucho/log.hpp>

namespace yella
{

namespace test
{

test::test(const std::filesystem::path& test_file, const std::filesystem::path& plugin)
    : doc(YAML::LoadFile(test_file.string()))
{
    auto type = doc["plugin.type"];
    if (!type)
        throw std::runtime_error(std::string("Could not find 'plugin.type' in '") + test_file.string() + "'");
    if (type.as<std::string>() != "file")
        throw std::runtime_error(std::string("Only 'plugin.type' of 'file' is supported: Found '") + type.as<std::string>() + "'");
    doc.remove("plugin.type");
    doc.remove("chucho::logger");
    impl_ = std::make_unique<file_test_impl>(doc, plugin);
}

bool test::run()
{
    bool result = true;
    try
    {
        impl_->run();
    }
    catch (const std::exception& e)
    {
        CHUCHO_ERROR_L("Test failed: " << e.what());
        result = false;

    }
    return result;
}

}

}