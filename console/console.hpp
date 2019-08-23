#ifndef YELLA_CONSOLE_HPP__
#define YELLA_CONSOLE_HPP__

#include "model.hpp"
#include "main_window.hpp"
#include "message_queue.hpp"

namespace yella
{

namespace console
{

class console
{
public:
    console(const configuration& cnf);

    int run(int argc, char* argv[]);

private:
    const configuration& config_;
    std::unique_ptr<database> db_;
    main_window main_window_;
    model model_;
    std::unique_ptr<message_queue> mq_;
};

}

}

#endif
