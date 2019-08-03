#ifndef YELLA_RABBITMQ_HPP__
#define YELLA_RABBITMQ_HPP__

#include "message_queue.hpp"
#include <amqp.h>
#include <atomic>

namespace yella
{

namespace console
{

class rabbitmq : public message_queue
{
public:
    rabbitmq(const configuration& cnf, handler heartbeat_handler, handler file_change_handler);

private:
    amqp_connection_state_t create_connection();
    void receiver_main();

    std::thread receiver_;
    std::atomic_bool should_stop_;
};

}

}

#endif
