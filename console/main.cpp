#include "console.hpp"
#include <chucho/log.hpp>

int main(int argc, char* argv[])
{
    int rc;
    try
    {
        yella::console::configuration config(argc, argv);
        QApplication app(argc, argv);
        yella::console::console cnsl(config);
        rc = app.exec();
    }
    catch (const std::exception& e)
    {
        CHUCHO_FATAL(chucho::logger::get("console"), "Unexpected: " << e.what());
        rc = EXIT_FAILURE;
    }
    return rc;
}