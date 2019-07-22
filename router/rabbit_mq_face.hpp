#ifndef YELLA_RABBIT_MQ_FACE_HPP__
#define YELLA_RABBIT_MQ_FACE_HPP__

#include "mq_face.hpp"
#include <amqp.h>
#include <map>
#include <thread>
#include <set>

namespace yella
{

namespace router
{

class rabbit_mq_face : public mq_face
{
public:
    rabbit_mq_face(const configuration& cnf);
    ~rabbit_mq_face();

    void send(const std::uint8_t* const msg, std::size_t len) override;

private:
    class sender
    {
    public:
        sender(const configuration& cnf);
        sender(const sender&) = delete;
        ~sender();

        sender& operator= (const sender&) = delete;

        void send(const std::uint8_t* const msg, std::size_t len);

    private:
        const configuration& config_;
        amqp_connection_state_t cxn_;
        std::set<std::string> exchanges_;
    };

    std::map<std::thread::id, sender> senders_;
};

}

}

#endif
