#ifndef YELLA_ZEROMQ_AGENT_FACE_HPP__
#define YELLA_ZEROMQ_AGENT_FACE_HPP__

#include "agent_face.hpp"
#include <zmq.hpp>
#include <atomic>
#include <thread>
#include <map>

namespace yella
{

namespace router
{

class zeromq_agent_face : public agent_face
{
public:
    zeromq_agent_face(const configuration& cnf);
    ~zeromq_agent_face();

    void run(std::shared_ptr<face> other_face,
             std::function<void()> callback_of_death) override;
    void send(const std::uint8_t* const msg, std::size_t len) override;

private:
    void backend_main();
    void worker_main();

    zmq::context_t context_;
    std::atomic_bool should_stop_;
    std::thread worker_;
    std::map<std::thread::id, zmq::socket_t> senders_;
};

}

}

#endif
