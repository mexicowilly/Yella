#include "rabbitmq.hpp"
#include "fatal_error.hpp"
#include "parcel_generated.h"
#include <amqp_tcp_socket.h>
#include <chucho/log.hpp>

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
    throw yella::console::fatal_error(stream.str());
}

}

namespace yella
{

namespace console
{

rabbitmq::rabbitmq(const configuration& cnf, handler heartbeat_handler, handler file_change_handler)
    : message_queue(cnf, heartbeat_handler, file_change_handler)
{
}

amqp_connection_state_t rabbitmq::create_connection()
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
        CHUCHO_INFO_L("Connected to RabbitMQ server");
    }
    catch (const std::exception& e)
    {
        amqp_destroy_connection(result);
        throw;
    }
    return result;
}

void rabbitmq::receiver_main()
{
    CHUCHO_INFO_L_STR("Message queue receiver is starting");
    amqp_connection_state_t cxn = nullptr;
    try
    {
        cxn = create_connection();
    }
    catch (...)
    {

    }
    if (cxn != nullptr)
    {
        amqp_channel_close(cxn, YELLA_CHANNEL, AMQP_REPLY_SUCCESS);
        amqp_connection_close(cxn, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(cxn);
    }
    CHUCHO_INFO_L_STR("Message queue receiver is ending");
}

}

}
