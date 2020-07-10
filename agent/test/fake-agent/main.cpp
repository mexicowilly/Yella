#include "test.hpp"
#include "cxxopts.hpp"
#include <chucho/configuration.hpp>
#include <chucho/finalize.hpp>
#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[])
{
    int rc = EXIT_SUCCESS;
    try
    {
        cxxopts::Options opts("fake-agent", "Tests plugins");
        opts.add_options()
            ("t,test", "The test file", cxxopts::value<std::string>())
            ("p,plugin", "The plugin to test", cxxopts::value<std::string>());
        auto result = opts.parse(argc, argv);
        if (result["test"].count() && result["plugin"].count())
        {
            chucho::configuration::set_file_name(result["test"].as<std::string>());
            yella::test::test tst(result["test"].as<std::string>(), result["plugin"].as<std::string>());
            tst.run();
        }
        else
        {
            std::cout << opts.help() << std::endl;
            rc = EXIT_FAILURE;
        }
    }
    catch (const std::exception& e)
    {
        rc = EXIT_FAILURE;
    }
    chucho::finalize();
    return rc;
}