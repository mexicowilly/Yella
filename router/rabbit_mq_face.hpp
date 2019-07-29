#ifndef YELLA_RABBIT_MQ_FACE_HPP__
#define YELLA_RABBIT_MQ_FACE_HPP__

#include "mq_face.hpp"
#include <amqp.h>
#include <map>
#include <thread>
#include <set>
#include <atomic>

namespace yella
{

namespace router
{

class rabbit_mq_face : public mq_face
{
public:
    rabbit_mq_face(const configuration& cnf);
    ~rabbit_mq_face();

    void run(std::shared_ptr<face> other_face,
             std::function<void()> callback_of_death) override;
    void send(const std::uint8_t* const msg, std::size_t len) override;

private:
    class sender
    {
    public:
        sender(rabbit_mq_face& rmf);
        sender(const sender&) = delete;
        ~sender();

        sender& operator= (const sender&) = delete;

        void send(const std::uint8_t* const msg, std::size_t len);

    private:
        const configuration& config_;
        amqp_connection_state_t cxn_;
        std::set<std::string> exchanges_;
    };

    amqp_connection_state_t create_connection();
    void receiver_main();

    std::map<std::thread::id, sender> senders_;
    std::thread receiver_;
    std::atomic_bool should_stop_;
};

}

}

#endif
