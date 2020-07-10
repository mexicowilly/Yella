#ifndef YELLA_TEST_HPP__
#define YELLA_TEST_HPP__

#include "test_impl.hpp"
#include <chucho/loggable.hpp>
#include <yaml-cpp/yaml.h>
#include <filesystem>

namespace yella
{

namespace test
{

class test : public chucho::loggable<test>
{
public:
    test(const std::filesystem::path& test_file, const std::filesystem::path& plugin);

    void run();

protected:
    YAML::Node doc;
    std::unique_ptr<test_impl> impl_;
};

}

}

#endif
