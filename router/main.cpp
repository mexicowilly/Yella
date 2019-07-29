#include "bucket_brigade.hpp"
extern "C"
{
#include "agent/signal_handler.h"
}
#include <chucho/finalize.hpp>
#include <chucho/log.hpp>

namespace
{

void termination_handler(void* arg)
{
    reinterpret_cast<yella::router::bucket_brigade*>(arg)->termination_handler();
}

}

int main(int argc, char* argv[])
{
    int rc = EXIT_SUCCESS;

    try
    {
        install_signal_handler();
        yella::router::configuration config(argc, argv);
        yella::router::bucket_brigade bb(config);
        set_signal_termination_handler(termination_handler, &bb);
        bb.run();
    }
    catch (const std::exception& e)
    {
        CHUCHO_ERROR(chucho::logger::get("yella.router"), "Unexpected error: " << e.what());
        rc = EXIT_FAILURE;
    }
    chucho::finalize();
    return rc;
}