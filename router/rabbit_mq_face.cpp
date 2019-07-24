#include "rabbit_mq_face.hpp"
#include "parcel_generated.h"
#include "fatal_error.hpp"
#include <amqp_tcp_socket.h>
#include <chucho/log.hpp>
#include <sstream>

namespace
{

constexpr const int YELLA_CHANNEL = 1;

void respond(const amqp_rpc_reply_t& rep)
{
    if (rep.reply_type == AMQP_RESPONSE_NORMAL)
        return;
    std::ostringstream stream;
    stream << "The RabbitMQ broker reports an error: ";
    if (rep.reply_type == AMQP_RESPONSE_NONE)
    {
        stream << "Missing reply type";
    }
    else if (rep.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION)
    {
        stream << amqp_error_string2(rep.library_error);
    }
    else if (rep.reply_type == AMQP_RESPONSE_SERVER_EXCEPTION)
    {
        if (rep.reply.id == AMQP_CHANNEL_CLOSE_METHOD)
        {
            auto ch = reinterpret_cast<amqp_channel_close_t*>(rep.reply.decoded);
            stream << "Channel: (" << ch->reply_code << ") " <<
                   std::string(reinterpret_cast<char*>(ch->reply_text.bytes), ch->reply_text.len);
        }
        else if (rep.reply.id == AMQP_CONNECTION_CLOSE_METHOD)
        {
            auto cn = reinterpret_cast<amqp_connection_close_t*>(rep.reply.decoded);
            stream << "Connection: (" << cn->reply_code << ") " <<
                   std::string(reinterpret_cast<char*>(cn->reply_text.bytes), cn->reply_text.len);
        }
        else
        {
            stream << "Unknown method: " << rep.reply.id;
        }
    }
    throw yella::router::fatal_error(stream.str());
}

}

namespace yella
{

namespace router
{

rabbit_mq_face::rabbit_mq_face(const configuration& cnf)
    : mq_face(cnf),
      should_stop_(false)
{
}

rabbit_mq_face::~rabbit_mq_face()
{
}

amqp_connection_state_t rabbit_mq_face::create_connection()
{
    amqp_connection_state_t result;
    try
    {
        amqp_connection_info info;
        std::vector<char> url(config_.mq_broker().c_str(), config_.mq_broker().c_str() + config_.mq_broker().length() + 1);
        int rc = amqp_parse_url(&url[0], &info);
        if (rc != AMQP_STATUS_OK)
            throw std::invalid_argument("Invalid AMQP URL: " + config_.mq_broker());
        if (info.ssl)
            throw std::invalid_argument("SSL is not supported: " + config_.mq_broker());
        result = amqp_new_connection();
        amqp_socket_t* sock = amqp_tcp_socket_new(result);
        if (sock == nullptr)
            throw std::runtime_error("Unable to create TCP socket");
        rc = amqp_socket_open(sock, info.host, info.port);
        if (rc != AMQP_STATUS_OK)
            throw std::runtime_error(std::string("Unable to open socket: ") + amqp_error_string2(rc));
        amqp_rpc_reply_t rep = amqp_login(result,
                                          info.vhost,
                                          0,
                                          AMQP_DEFAULT_FRAME_SIZE,
                                          0,
                                          AMQP_SASL_METHOD_PLAIN,
                                          info.user,
                                          info.password);
        respond(rep);
        amqp_channel_open(result, YELLA_CHANNEL);
        rep = amqp_get_rpc_reply(result);
        respond(rep);
    }
    catch (const std::exception& e)
    {
        amqp_destroy_connection(result);
        throw;
    }
    return result;
}

void rabbit_mq_face::receiver_main()
{
    CHUCHO_INFO_L_STR("Message queue receiver is starting");
    amqp_connection_state_t cxn = nullptr;
    try
    {
        cxn = create_connection();
        for (const auto& q : config_.consumption_queues())
        {
            amqp_bytes_t qbytes;
            qbytes.bytes = const_cast<char*>(q.data());
            qbytes.len = q.length();
            amqp_basic_consume(cxn,
                               YELLA_CHANNEL,
                               qbytes,
                               amqp_empty_bytes,
                               0, // no local
                               1, // no ack
                               0, // exclusive
                               amqp_empty_table);
            auto rep = amqp_get_rpc_reply(cxn);
            respond(rep);
        }
        amqp_envelope_t env;
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 500000;
        while (!should_stop_)
        {
            amqp_maybe_release_buffers(cxn);
            auto rep = amqp_consume_message(cxn, &env, &timeout, 0);
            if (rep.reply_type != AMQP_RESPONSE_NONE)
            {
                respond(rep);
                other_face_->send(reinterpret_cast<uint8_t*>(env.message.body.bytes), env.message.body.len);
                amqp_destroy_envelope(&env);
            }
        }
    }
    catch (const std::exception& e)
    {
        CHUCHO_FATAL_L("Error occurred with broker: " << e.what());
    }
    if (cxn != nullptr)
    {
        amqp_channel_close(cxn, YELLA_CHANNEL, AMQP_REPLY_SUCCESS);
        amqp_connection_close(cxn, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(cxn);
    }
    CHUCHO_INFO_L_STR("Message queue receiver is ending");
}

void rabbit_mq_face::run(std::shared_ptr<face> other_face)
{
    mq_face::run(other_face);
    receiver_ = std::thread(&rabbit_mq_face::receiver_main, this);
}

void rabbit_mq_face::send(const std::uint8_t* const msg, std::size_t len)
{
    auto found = senders_.find(std::this_thread::get_id());
    if (found == senders_.end())
    {
        found = senders_.emplace(std::piecewise_construct,
                                 std::forward_as_tuple(std::this_thread::get_id()),
                                 std::forward_as_tuple(*this)).first;
    }
    found->second.send(msg, len);
}

rabbit_mq_face::sender::sender(rabbit_mq_face& rmf)
    : config_(rmf.config_),
      cxn_(rmf.create_connection())
{
}

rabbit_mq_face::sender::~sender()
{
    amqp_channel_close(cxn_, YELLA_CHANNEL, AMQP_REPLY_SUCCESS);
    amqp_connection_close(cxn_, AMQP_REPLY_SUCCESS);
    amqp_destroy_connection(cxn_);
}

void rabbit_mq_face::sender::send(const std::uint8_t* const msg, std::size_t len)
{
    auto parcel = yella::fb::Getparcel(msg);
    auto type = parcel->type();
    if (type == nullptr)
    {
        throw std::runtime_error("No type included in the parcel");
    }
    else
    {
        auto tstr = type->str();
        auto xbytes = amqp_cstring_bytes(tstr.c_str());
        auto found = exchanges_.find(tstr);
        if (found == exchanges_.end())
        {
            amqp_exchange_declare(cxn_,
                                  YELLA_CHANNEL,
                                  xbytes,
                                  amqp_cstring_bytes("amq.fanout"),
                                  0, // passive
                                  1, // durable
                                  0, // auto-delete
                                  0, // internal
                                  amqp_empty_table);
            amqp_rpc_reply_t rep = amqp_get_rpc_reply(cxn_);
            respond(rep);
            if (config_.bind_exchanges())
            {
                amqp_queue_declare(cxn_,
                                   YELLA_CHANNEL,
                                   xbytes,
                                   0, // passive
                                   1, // durable
                                   0, // exclusive
                                   0, // auto-delete
                                   amqp_empty_table);
                rep = amqp_get_rpc_reply(cxn_);
                respond(rep);
                amqp_queue_bind(cxn_,
                                YELLA_CHANNEL,
                                xbytes,
                                xbytes,
                                amqp_empty_bytes,
                                amqp_empty_table);
                rep = amqp_get_rpc_reply(cxn_);
                respond(rep);
            }
            exchanges_.insert(tstr);
        }
        amqp_bytes_t mbytes;
        mbytes.bytes = const_cast<uint8_t*>(msg);
        mbytes.len = len;
        int rc = amqp_basic_publish(cxn_,
                                    YELLA_CHANNEL,
                                    xbytes,
                                    amqp_empty_bytes,
                                    0, // mandatory
                                    0, // immediate
                                    nullptr, // properties
                                    mbytes);
        if (rc != AMQP_STATUS_OK)
            throw std::runtime_error(std::string("Error publishing to RabbitMQ: ") + amqp_error_string2(rc));
    }
}

}

}