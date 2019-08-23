#include "rabbitmq.hpp"

namespace yella
{

namespace console
{

message_queue::message_queue(const configuration& cnf, model& mdl)
    : config_(cnf)
{
    QObject::connect(this, SIGNAL(file_changed(const parcel&)),
                     &mdl, SLOT(file_changed(const parcel&)));
    QObject::connect(this, SIGNAL(hearbeat(const parcel&)),
                     &mdl, SLOT(heartbeat(const parcel&)));
}

message_queue::~message_queue()
{
    for (auto& thr : receivers_)
        thr.join();
}

std::unique_ptr<message_queue> message_queue::create(const configuration& cnf, model& mdl)
{
    if (cnf.mq_type() == "rabbitmq")
        return std::make_unique<rabbitmq>(cnf, mdl);
    throw std::invalid_argument("Unrecognized message queue type: " + cnf.mq_type());
}

}

}
