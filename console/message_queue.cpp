#include "rabbitmq.hpp"

namespace yella
{

namespace console
{

message_queue::message_queue(const configuration& cnf,
                             handler heartbeat_handler,
                             handler file_change_handler,
                             death_callback dc)
    : config_(cnf),
      heartbeat_handler_(heartbeat_handler),
      file_change_handler_(file_change_handler),
      death_callback_(dc)
{
}

message_queue::~message_queue()
{
    for (auto& thr : receivers_)
        thr.join();
}

std::unique_ptr<message_queue> message_queue::create(const configuration& cnf,
                                                     handler heartbeat_handler,
                                                     handler file_change_handler,
                                                     death_callback dc)
{
    if (cnf.mq_type() == "rabbitmq")
        return std::make_unique<rabbitmq>(cnf, heartbeat_handler, file_change_handler, dc);
    throw std::invalid_argument("Unrecognized message queue type: " + cnf.mq_type());
}

}

}
