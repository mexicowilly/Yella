#include "console.hpp"
#include <chucho/log.hpp>

int main(int argc, char* argv[])
{
    int rc;
    try
    {
        yella::console::configuration config(argc, argv);
        yella::console::console cons(config);
        rc = cons.run(argc, argv);
    }
    catch (const std::exception& e)
    {
        CHUCHO_FATAL(chucho::logger::get("yella.console"), "Unexpected top-level exception: " << e.what());
        rc = EXIT_FAILURE;
    }
    return rc;
}