#include "rabbitmq.hpp"

namespace yella
{

namespace console
{

message_queue::message_queue(const configuration& cnf, handler heartbeat_handler, handler file_change_handler)
    : config_(cnf),
      heartbeat_handler_(heartbeat_handler),
      file_change_handler_(file_change_handler)
{
}

message_queue::~message_queue()
{
}

std::unique_ptr<message_queue> message_queue::create_message_queue(const configuration& cnf,
                                                                   handler heartbeat_handler,
                                                                   handler file_change_handler)
{
    if (cnf.mq_type() == "rabbitmq")
        return std::make_unique<rabbitmq>(cnf, heartbeat_handler, file_change_handler);
    throw std::invalid_argument("Unrecognized message queue type: " + cnf.mq_type());
}

}

}
