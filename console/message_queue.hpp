#ifndef YELLA_MESSAGE_QUEUE_HPP__
#define YELLA_MESSAGE_QUEUE_HPP__

#include "configuration.hpp"
#include "parcel.hpp"
#include <chucho/loggable.hpp>
#include <thread>

namespace yella
{

namespace console
{

class message_queue : public chucho::loggable<message_queue>
{
public:
    typedef std::function<void(const parcel&)> handler;

    static std::unique_ptr<message_queue> create_message_queue(const configuration& cnf,
                                                               handler heartbeat_handler,
                                                               handler file_change_handler);

    message_queue(const message_queue&) = delete;
    virtual ~message_queue();

    message_queue& operator= (const message_queue&) = delete;

protected:
    message_queue(const configuration& cnf, handler heartbeat_handler, handler file_change_handler);

    const configuration& config_;
    handler heartbeat_handler_;
    handler file_change_handler_;
    std::vector<std::thread> workers_;
};

}

}

#endif
