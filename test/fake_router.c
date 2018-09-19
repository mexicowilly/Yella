#include "agent/signal_handler.h"
#include <zmq.h>

int main(int argc, char* argv[])
{
    install_signal_handler();
    return 0;
}
