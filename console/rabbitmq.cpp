#include "rabbitmq.hpp"
#include "fatal_error.hpp"
#include "parcel.hpp"
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

rabbitmq::rabbitmq(const configuration& cnf,
                   handler heartbeat_handler,
                   handler file_change_handler,
                   death_callback dc)
    : message_queue(cnf, heartbeat_handler, file_change_handler, dc)
{
    for (auto& thr : receivers_)
        thr = std::thread(&rabbitmq::receiver_main, this);
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
        while (!should_stop_)
        {
            timeout.tv_sec = 0;
            timeout.tv_usec = 500000;
            amqp_maybe_release_buffers(cxn);
            auto rep = amqp_consume_message(cxn, &env, &timeout, 0);
            if (rep.reply_type == AMQP_RESPONSE_LIBRARY_EXCEPTION && rep.library_error == AMQP_STATUS_TIMEOUT)
                continue;
            respond(rep);
            parcel pcl(reinterpret_cast<std::uint8_t*>(env.message.body.bytes));
            amqp_destroy_envelope(&env);
            try
            {
                if (pcl.type() == "yella.agent.heartbeat")
                    heartbeat_handler_(pcl);
                else if (pcl.type() == "yella.file.change")
                    file_change_handler_(pcl);
                else
                    CHUCHO_ERROR_L("Unknown message type: " << pcl.type());
            }
            catch (const std::exception& e)
            {
                CHUCHO_ERROR_L("Error processing '" << pcl.type() << "': " << e.what());
            }
        }
    }
    catch (const std::exception& e)
    {
        CHUCHO_FATAL_L("Error occurred with broker: " << e.what());
        death_callback_();
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
