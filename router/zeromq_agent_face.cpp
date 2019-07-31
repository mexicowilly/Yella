#include "zeromq_agent_face.hpp"
#include "parcel_generated.h"
#include "fatal_error.hpp"
#include <chucho/log.hpp>
#include <sstream>
#include <queue>

using namespace std::chrono_literals;

namespace
{

constexpr const char* BACKEND_ADDR = "inproc://backend";
constexpr const char* OUTGOING_ADDR = "inproc://outgoing";

}

namespace yella
{

namespace router
{

zeromq_agent_face::zeromq_agent_face(const configuration& cnf)
    : agent_face(cnf),
      should_stop_(false)
{
}

zeromq_agent_face::~zeromq_agent_face()
{
    should_stop_ = true;
    worker_.join();
}

void zeromq_agent_face::backend_main()
{
    CHUCHO_INFO_L("Back-end thread '" << std::this_thread::get_id() << "' starting");
    try
    {
        zmq::socket_t sock(context_, zmq::socket_type::req);
        auto id = std::hash<std::thread::id>()(std::this_thread::get_id());
        sock.setsockopt(ZMQ_IDENTITY, id);
        sock.connect(BACKEND_ADDR);
        zmq::message_t empty_msg;
        sock.send(empty_msg);
        zmq::pollitem_t pi;
        pi.socket = static_cast<void*>(sock);
        pi.fd = 0;
        pi.events = ZMQ_POLLIN;
        while (!should_stop_)
        {
            int rc = zmq::poll(&pi, 1, 500ms);
            if (should_stop_)
                break;
            if (rc > 0)
            {
                zmq::message_t msg;
                if (sock.recv(&msg, ZMQ_DONTWAIT))
                {
                    try
                    {
                        other_face_->send(reinterpret_cast<uint8_t*>(msg.data()), msg.size());
                        sock.send(empty_msg);
                    }
                    catch (const fatal_error& fe)
                    {
                        CHUCHO_FATAL_L("Fatal error: " << fe.what());
                        should_stop_ = true;
                    }
                    catch (const std::exception e)
                    {
                        CHUCHO_ERROR_L("Error sending message to message queue: " << e.what());
                    }
                }
            }
        }
    }
    catch (const std::exception& e)
    {
        CHUCHO_FATAL_L("Error setting up back-end connection: " << e.what());

    }
    CHUCHO_INFO_L("Back-end thread '" << std::this_thread::get_id() << "' ending");
}

void zeromq_agent_face::run(face* other_face,
                            std::function<void()> callback_of_death)
{
    agent_face::run(other_face, callback_of_death);
    worker_ = std::thread(&zeromq_agent_face::worker_main, this);
}

void zeromq_agent_face::send(const std::uint8_t* const msg, std::size_t len)
{
    auto found = senders_.find(std::this_thread::get_id());
    if (found == senders_.end())
    {
        found = senders_.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(std::this_thread::get_id()),
                                 std::forward_as_tuple(context_, zmq::socket_type::push)).first;
        found->second.connect(OUTGOING_ADDR);
    }
    auto recipient = yella::fb::Getparcel(msg)->recipient();
    if (recipient == NULL)
    {
        CHUCHO_ERROR_L_STR("No recipient was found in the message");
    }
    else
    {
        zmq::message_t id(recipient->c_str(), std::strlen(recipient->c_str()));
        found->second.send(id, ZMQ_SNDMORE);
        zmq::message_t delim;
        found->second.send(delim, ZMQ_SNDMORE);
        zmq::message_t zmsg(msg, len);
        found->second.send(zmsg);
    }
}

void zeromq_agent_face::worker_main()
{
    CHUCHO_INFO_L_STR("ZeroMQ worker thread starting");
    try
    {
        zmq::socket_t frontend_sock(context_, zmq::socket_type::router);
        zmq::socket_t backend_sock(context_, zmq::socket_type::router);
        zmq::socket_t outgoing_sock(context_, zmq::socket_type::pull);
        auto stream = std::make_unique<std::ostringstream>();
        *stream << "tcp://*:" << config_.agent_port();
        frontend_sock.bind(stream->str());
        stream.reset();
        backend_sock.bind(BACKEND_ADDR);
        outgoing_sock.bind(OUTGOING_ADDR);
        std::vector<std::thread> backend_threads;
        for (auto i = 0; i < config_.worker_threads(); i++)
            backend_threads.emplace_back(std::thread(&zeromq_agent_face::backend_main, this));
        std::queue<std::size_t> ready_workers;
        zmq::pollitem_t pi[3];
        pi[0].socket = static_cast<void*>(backend_sock);
        pi[0].fd = 0;
        pi[0].events = ZMQ_POLLIN;
        pi[1].socket = static_cast<void*>(outgoing_sock);
        pi[1].fd = 0;
        pi[1].events = ZMQ_POLLIN;
        pi[2].socket = static_cast<void*>(frontend_sock);
        pi[2].fd = 0;
        pi[2].events = ZMQ_POLLIN;
        std::size_t err_count = 0;
        zmq::message_t id;
        zmq::message_t delim;
        zmq::message_t msg;
        while (!should_stop_)
        {
            // Clear this one, because if it isn't used, then it won't be cleared by zmq::poll
            pi[2].revents = 0;
            int rc = zmq::poll(pi, (ready_workers.empty() ? 2 : 3), 500ms);
            if (should_stop_)
                break;
            if (rc > 0)
            {
                try
                {
                    // Backend socket
                    if (pi[0].revents & ZMQ_POLLIN)
                    {
                        if (!backend_sock.recv(&id, ZMQ_DONTWAIT))
                            throw "backend ID";
                        assert(id.size() == sizeof(std::size_t));
                        if (!backend_sock.recv(&delim, ZMQ_DONTWAIT))
                            throw "backend delimiter";
                        assert(delim.size() == 0);
                        if (!backend_sock.recv(&msg, ZMQ_DONTWAIT))
                            throw "backend empty";
                        assert(msg.size() == 0);
                        ready_workers.push(*static_cast<std::size_t*>(id.data()));
                    }
                    // Outgoing socket
                    if (pi[1].revents & ZMQ_POLLIN)
                    {
                        if (!frontend_sock.recv(&id, ZMQ_DONTWAIT))
                            throw "agent ID";
                        if (!frontend_sock.recv(&delim, ZMQ_DONTWAIT))
                            throw "agent delimiter";
                        assert(delim.size() == 0);
                        if (!frontend_sock.recv(&msg, ZMQ_DONTWAIT))
                            throw "agent message";
                        frontend_sock.send(id, ZMQ_SNDMORE);
                        frontend_sock.send(delim, ZMQ_SNDMORE);
                        frontend_sock.send(msg);
                    }
                    // Agent socket
                    if (pi[2].revents & ZMQ_POLLIN)
                    {
                        if (!frontend_sock.recv(&id, ZMQ_DONTWAIT))
                            throw "agent ID";
                        if (!frontend_sock.recv(&delim, ZMQ_DONTWAIT))
                            throw "agent delimiter";
                        assert(delim.size() == 0);
                        if (!frontend_sock.recv(&msg, ZMQ_DONTWAIT))
                            throw "agent message";
                        err_count = 0;
                        auto cur_backend = ready_workers.front();
                        ready_workers.pop();
                        id.rebuild(&cur_backend, sizeof(cur_backend));
                        backend_sock.send(id, ZMQ_SNDMORE);
                        backend_sock.send(delim, ZMQ_SNDMORE);
                        backend_sock.send(msg);
                    }
                }
                catch (const char* const desc)
                {
                    CHUCHO_WARN_L("Unexpected lack of data to read: " << desc);
                    // A false return from recv means that data was not waiting to be read
                    if (++err_count == 3)
                        throw std::runtime_error("Too many read errors occurred on the socket");
                }
                catch (const std::exception& e)
                {
                    CHUCHO_ERROR_L("An error occurred in ZeroMQ: " << e.what());
                }
            }
        }
        for (auto& cur : backend_threads)
            cur.join();
    }
    catch (const std::exception& e)
    {
        CHUCHO_ERROR_L("Error in ZeroMQ worker: " << e.what());
        callback_of_death_();
    }
    CHUCHO_INFO_L_STR("ZeroMQ worker thread ending");
}

}

}