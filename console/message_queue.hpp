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
    using handler = std::function<void(const parcel&)>;
    using death_callback = std::function<void()>;

    static std::unique_ptr<message_queue> create(const configuration& cnf,
                                                 handler heartbeat_handler,
                                                 handler file_change_handler,
                                                 death_callback dc);

    message_queue(const message_queue&) = delete;
    virtual ~message_queue();

    message_queue& operator= (const message_queue&) = delete;

protected:
    message_queue(const configuration& cnf,
                  handler heartbeat_handler,
                  handler file_change_handler,
                  death_callback dc);

    const configuration& config_;
    handler heartbeat_handler_;
    handler file_change_handler_;
    death_callback death_callback_;
    std::vector<std::thread> receivers_;
};

}

}

#endif
