/*
 * Copyright 2016 Will Mason
 * 
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#include "router.h"
#include "common/settings.h"
#include "common/return_code.h"
#include "common/thread.h"
#include "common/text_util.h"
#include <chucho/log.h>
#include <zmq.h>
#include <stdlib.h>
#include <assert.h>

static const char* MONITOR_SOCKET = "inproc://monitor";
static const char* OUTGOING_SOCKET = "inproc://outgoing";
static long POLL_TIMEOUT_MILLIS = 500;

struct yella_router
{
    void* zmctx;
    yella_uuid* id;
    yella_router_state state;
    yella_router_state_callback state_callback;
    void* state_callback_data;
    yella_router_message_received_callback recv_callback;
    void* recv_callback_data;
    yella_mutex* mtx;
    bool stopped;
    yella_thread* worker_thread;
    chucho_logger_t* lgr;
};

struct yella_sender
{
    void* sock;
};

typedef struct monitor_event
{
    uint16_t id;
    uint32_t value;
    char endpoint[1024];
} monitor_event;

static void* create_monitor_socket(yella_router* rtr)
{
    void* sock;
    int rc;

    sock = zmq_socket(rtr->zmctx, ZMQ_PAIR);
    if (sock == NULL)
    {
        CHUCHO_C_ERROR_L(rtr->lgr,
                         "Unable to create the monitor socket",
                         zmq_strerror(zmq_errno()));
        return NULL;
    }
    rc = zmq_connect(sock, MONITOR_SOCKET);
    if (rc != 0)
    {
        CHUCHO_C_ERROR_L(rtr->lgr,
                         "Unable to connect to %s: %s",
                         MONITOR_SOCKET,
                         zmq_strerror(zmq_errno()));
        zmq_close(sock);
        return NULL;
    }
    return sock;
}

static void* create_router_socket(yella_router* rtr)
{
    void* sock;
    const UChar* endpoint;
    const char* id_text;
    int recon_timeout_millis;
    int rc;
    char* utf8;

    sock = NULL;
    endpoint = yella_settings_get_text(u"agent", u"router");
    if (endpoint == NULL)
    {
        CHUCHO_C_ERROR_L(rtr->lgr,
                         "No router has been defined in settings");
        return NULL;
    }
    sock = zmq_socket(rtr->zmctx, ZMQ_DEALER);
    if (sock == NULL)
    {
        CHUCHO_C_ERROR_L(rtr->lgr,
                         "The router socket could not be created");
        return NULL;
    }
    utf8 = yella_to_utf8(rtr->id->text);
    zmq_setsockopt(sock,
                   ZMQ_IDENTITY,
                   utf8,
                   strlen(utf8));
    free(utf8);
    recon_timeout_millis = *yella_settings_get_uint(u"agent", u"reconnect-timeout-seconds") * 1000;
    zmq_setsockopt(sock,
                   ZMQ_RECONNECT_IVL,
                   &recon_timeout_millis,
                   sizeof(recon_timeout_millis));
    rc = zmq_socket_monitor(sock, MONITOR_SOCKET, ZMQ_EVENT_ALL);
    if (rc == -1)
    {
        CHUCHO_C_ERROR_L(rtr->lgr,
                         "Could not create socket monitor at %s: %s",
                         MONITOR_SOCKET,
                         zmq_strerror(zmq_errno()));
        zmq_close(sock);
        return NULL;
    }
    utf8 = yella_to_utf8(endpoint);
    rc = zmq_connect(sock, utf8);
    if (rc != 0)
    {
        CHUCHO_C_ERROR_L(rtr->lgr,
                         "Could not initiate connection to %s: %s",
                         utf8,
                         zmq_strerror(rc));
        free(utf8);
        zmq_close(sock);
        return NULL;
    }
    free(utf8);
    return sock;
}

void* create_outgoing_reader_socket(yella_router* rtr)
{
    void* sock;
    int rc;

    sock = zmq_socket(rtr->zmctx, ZMQ_PULL);
    if (sock == NULL)
    {
        CHUCHO_C_ERROR_L(rtr->lgr,
                         "Unable to create the outgoing reader socket",
                         zmq_strerror(zmq_errno()));
        return NULL;
    }
    rc = zmq_bind(sock, OUTGOING_SOCKET);
    if (rc != 0)
    {
        CHUCHO_C_ERROR_L(rtr->lgr,
                         "Unable to bind to %s: %s",
                         OUTGOING_SOCKET,
                         zmq_strerror(zmq_errno()));
        zmq_close(sock);
        return NULL;
    }
    return sock;
}

static bool read_monitor_event(void* sock, monitor_event* evt)
{
    int rc;
    zmq_msg_t frame1;
    zmq_msg_t frame2;

    zmq_msg_init(&frame1);
    rc = zmq_msg_recv(&frame1, sock, 0);
    if (rc == -1)
    {
        zmq_msg_close(&frame1);
        return false;
    }
    assert(zmq_msg_size(&frame1) == sizeof(evt->id) + sizeof(evt->value));
    memcpy(&evt->id, zmq_msg_data(&frame1), sizeof(evt->id));
    memcpy(&evt->value, zmq_msg_data(&frame1) + sizeof(evt->id), sizeof(evt->value));
    zmq_msg_close(&frame1);
    zmq_msg_init(&frame2);
    rc = zmq_msg_recv(&frame2, sock, 0);
    if (rc == -1)
    {
        zmq_msg_close(&frame2);
        return false;
    }
    strncpy(evt->endpoint, zmq_msg_data(&frame2), zmq_msg_size(&frame2));
    if (zmq_msg_size(&frame2) < sizeof(evt->endpoint) - 1)
        evt->endpoint[zmq_msg_size(&frame2)] = 0;
    zmq_msg_close(&frame2);
    return true;
}

static void set_state(yella_router* rtr, yella_router_state state)
{
    /* The order of these depends on the order of the enum values */
    static const char* state_text[] =
    {
        "connection pending",
        "connected",
        "disconnected",
        "socket closed"
    };

    yella_lock_mutex(rtr->mtx);
    if (rtr->state != state)
    {
        CHUCHO_C_INFO_L(rtr->lgr,
                        "The router state is changing: %s ==> %s",
                        state_text[rtr->state],
                        state_text[state]);
        rtr->state = state;
        if (rtr->state_callback != NULL)
            rtr->state_callback(state, rtr->state_callback_data);
    }
    yella_unlock_mutex(rtr->mtx);
}

static yella_rc process_monitor_in_event(yella_router* rtr, void* mon_sock)
{
    monitor_event evt;

    if (!read_monitor_event(mon_sock, &evt))
        return YELLA_READ_ERROR;
    switch (evt.id)
    {
    case ZMQ_EVENT_CONNECTED:
        CHUCHO_C_INFO_L(rtr->lgr,
                        "Completed connection to %s",
                        evt.endpoint);
        set_state(rtr, YELLA_ROUTER_CONNECTED);
        break;
    case ZMQ_EVENT_CONNECT_DELAYED:
        CHUCHO_C_INFO_L(rtr->lgr,
                        "Connection to %s delayed",
                        evt.endpoint);
        set_state(rtr, YELLA_ROUTER_CONNECTION_PENDING);
        break;
    case ZMQ_EVENT_CONNECT_RETRIED:
        CHUCHO_C_INFO_L(rtr->lgr,
                        "Connection to %s will be retried in %u milliseconds",
                        evt.endpoint,
                        evt.value);
        set_state(rtr, YELLA_ROUTER_CONNECTION_PENDING);
        break;
    case ZMQ_EVENT_CLOSED:
        set_state(rtr, YELLA_ROUTER_SOCKET_CLOSED);
        break;
    case ZMQ_EVENT_DISCONNECTED:
        CHUCHO_C_INFO_L(rtr->lgr,
                        "Disconnected from %s",
                        evt.endpoint);
        set_state(rtr, YELLA_ROUTER_DISCONNECTED);
        break;
    }
    return YELLA_NO_ERROR;
}

static yella_rc process_outgoing_in_event(void* rtr_sock, void* out_sock)
{
    zmq_msg_t msg;
    int rc;
    int more;

    zmq_msg_init(&msg);
    rc = zmq_msg_send(&msg, rtr_sock, ZMQ_SNDMORE);
    if (rc == -1)
    {
        CHUCHO_C_ERROR("yella.router",
                       "Could not send message delimiter: %s",
                       zmq_strerror(zmq_errno()));
        zmq_msg_close(&msg);
        return YELLA_WRITE_ERROR;
    }
    more = 1;
    while (more)
    {
        zmq_msg_init(&msg);
        rc = zmq_msg_recv(&msg, out_sock, 0);
        if (rc == -1)
        {
            CHUCHO_C_ERROR("yella.router",
                           "Could not receive message part from outgoing pusher: %s",
                           zmq_strerror(zmq_errno()));
            zmq_msg_close(&msg);
            return YELLA_READ_ERROR;
        }
        more = zmq_msg_more(&msg);
        rc = zmq_msg_send(&msg, rtr_sock, (more == 0) ? 0 : ZMQ_SNDMORE);
        if (rc == -1)
        {
            CHUCHO_C_ERROR("yella.router",
                           "Could not send message part to router: %s",
                           zmq_strerror(zmq_errno()));
            zmq_msg_close(&msg);
            return YELLA_WRITE_ERROR;
        }
    }
    return YELLA_NO_ERROR;
}

static yella_rc process_router_in_event(yella_router* rtr, void* rtr_sock)
{
    zmq_msg_t delim;
    zmq_msg_t header;
    zmq_msg_t body;
    yella_message_part hpart;
    yella_message_part bpart;
    int rc;
    size_t overcount;

    overcount = 0;
    zmq_msg_init(&delim);
    rc = zmq_msg_recv(&delim, rtr_sock, 0);
    if (rc == -1)
    {
        CHUCHO_C_ERROR_L(rtr->lgr,
                         zmq_strerror(zmq_errno()));
        zmq_msg_close(&delim);
        return YELLA_READ_ERROR;
    }
    if (!zmq_msg_more(&delim))
    {
        CHUCHO_C_ERROR_L(rtr->lgr,
                         "An empty delimiter message part was expected followed by the header and body, but only the delimiter was found, size %zu",
                         zmq_msg_size(&delim));
        zmq_msg_close(&delim);
        return YELLA_READ_ERROR;
    }
    zmq_msg_close(&delim);
    zmq_msg_init(&header);
    rc = zmq_msg_recv(&header, rtr_sock, 0);
    if (rc == -1)
    {
        CHUCHO_C_ERROR_L(rtr->lgr,
                         "Error receiving message header: %s",
                         zmq_strerror(zmq_errno()));
        zmq_msg_close(&header);
        return YELLA_READ_ERROR;
    }
    if (zmq_msg_more(&header))
    {
        zmq_msg_init(&body);
        rc = zmq_msg_recv(&body, rtr_sock, 0);
        if (rc == -1)
        {
            CHUCHO_C_ERROR_L(rtr->lgr,
                             "Error receiving message body: %s",
                             zmq_strerror(zmq_errno()));
            zmq_msg_close(&header);
            zmq_msg_close(&body);
            return YELLA_READ_ERROR;
        }
        while (zmq_msg_more(&body))
        {
            overcount++;
            if (zmq_msg_recv(&body, rtr_sock, 0) == -1)
                break;
        }
        if (overcount > 0)
        {
            CHUCHO_C_ERROR_L(rtr->lgr,
                             "Only two message parts are expected from the router. %zu extra message parts were found. All parts are being discarded.",
                             overcount);
            zmq_msg_close(&header);
            zmq_msg_close(&body);
            return YELLA_READ_ERROR;
        }
    }
    else
    {
        CHUCHO_C_ERROR_L(rtr->lgr,
                         "Two message parts are required when reading messages from the router, but only one was found");
        zmq_msg_close(&header);
        return YELLA_READ_ERROR;
    }
    if (rtr->recv_callback != NULL)
    {
        hpart.data = zmq_msg_data(&header);
        hpart.size = zmq_msg_size(&header);
        bpart.data = zmq_msg_data(&body);
        bpart.size = zmq_msg_size(&body);
        rtr->recv_callback(&hpart, &bpart, rtr->recv_callback_data);
    }
    zmq_msg_close(&header);
    zmq_msg_close(&body);
    return YELLA_NO_ERROR;
}

static void socket_worker_main(void* arg)
{
    yella_router* rtr;
    void* rtr_sock;
    void* mon_sock;
    void* out_sock;
    zmq_pollitem_t pis[3];
    int poll_count;
    bool stopped;

    rtr = (yella_router*)arg;
    CHUCHO_C_INFO_L(rtr->lgr,
                    "The socket worker thread is starting");
    rtr_sock = NULL;
    out_sock = NULL;
    mon_sock = NULL;
    rtr_sock = create_router_socket(rtr);
    if (rtr_sock == NULL)
        goto thread_exit;
    mon_sock = create_monitor_socket(rtr);
    if (mon_sock == NULL)
        goto thread_exit;
    out_sock = create_outgoing_reader_socket(rtr);
    if (out_sock == NULL)
        goto thread_exit;
    pis[0].socket = rtr_sock;
    pis[0].events = ZMQ_POLLIN;
    pis[1].socket = out_sock;
    pis[1].events = ZMQ_POLLIN;
    pis[2].socket = mon_sock;
    pis[2].events = ZMQ_POLLIN;
    while (true)
    {
        poll_count = zmq_poll(pis, 3, POLL_TIMEOUT_MILLIS);
        yella_lock_mutex(rtr->mtx);
        stopped = rtr->stopped;
        yella_unlock_mutex(rtr->mtx);
        if (stopped)
            break;
        if (poll_count > 0)
        {
            if ((pis[0].revents & ZMQ_POLLIN) != 0 && process_router_in_event(rtr, rtr_sock) != YELLA_NO_ERROR)
                break;
            if ((pis[1].revents & ZMQ_POLLIN) != 0 && process_outgoing_in_event(rtr_sock, out_sock) != YELLA_NO_ERROR)
                break;
            if ((pis[2].revents & ZMQ_POLLIN) != 0 && process_monitor_in_event(rtr, mon_sock) != YELLA_NO_ERROR)
                break;
        }
    }

thread_exit:
    if (rtr_sock != NULL)
        zmq_close(rtr_sock);
    if (out_sock != NULL)
        zmq_close(out_sock);
    if (mon_sock != NULL)
        zmq_close(mon_sock);
    CHUCHO_C_INFO_L(rtr->lgr,
                    "The socket worker thread is ending");
}

yella_router* yella_create_router(yella_uuid* id)
{
    yella_router* result;

    result = malloc(sizeof(yella_router));
    result->zmctx = zmq_ctx_new();
    result->id = id;
    result->state = YELLA_ROUTER_CONNECTION_PENDING;
    result->state_callback = NULL;
    result->state_callback_data = NULL;
    result->recv_callback = NULL;
    result->recv_callback_data = NULL;
    result->mtx = yella_create_mutex();
    result->stopped = false;
    result->lgr = chucho_get_logger("yella.router");
    result->worker_thread = yella_create_thread(socket_worker_main, result);
    return result;
}

yella_sender* yella_create_sender(yella_router* rtr)
{
    yella_sender* result;
    int rc;

    result = malloc(sizeof(yella_sender));
    result->sock = zmq_socket(rtr->zmctx, ZMQ_PUSH);
    if (result->sock == NULL)
    {
        CHUCHO_C_ERROR_L(rtr->lgr,
                         "Unable to create the sender socket",
                         zmq_strerror(zmq_errno()));
        return NULL;
    }
    rc = zmq_connect(result->sock, OUTGOING_SOCKET);
    if (rc != 0)
    {
        CHUCHO_C_ERROR_L(rtr->lgr,
                         "Unable to connect sender to %s: %s",
                         MONITOR_SOCKET,
                         zmq_strerror(zmq_errno()));
        zmq_close(result->sock);
        return NULL;
    }
    return result;
}

void yella_destroy_router(yella_router* rtr)
{
    if (rtr != NULL)
    {
        yella_lock_mutex(rtr->mtx);
        rtr->stopped = true;
        yella_unlock_mutex(rtr->mtx);
        yella_join_thread(rtr->worker_thread);
        yella_destroy_thread(rtr->worker_thread);
        yella_destroy_mutex(rtr->mtx);
        zmq_ctx_term(rtr->zmctx);
        chucho_release_logger(rtr->lgr);
        free(rtr);
    }
}

void yella_destroy_sender(yella_sender* sndr)
{
    if (sndr != NULL && sndr->sock != NULL)
        zmq_close(sndr->sock);
    free(sndr);
}

static void zmq_free(void* data, void* a)
{
    free(data);
}

yella_router_state yella_router_get_state(yella_router* rtr)
{
    yella_router_state st;

    yella_lock_mutex(rtr->mtx);
    st = rtr->state;
    yella_unlock_mutex(rtr->mtx);
    return st;
}

bool yella_send(yella_sender* sndr, yella_message_part* msgs, size_t count)
{
    size_t i;
    zmq_msg_t msg;
    int rc;

    for (i = 0; i < count; i++)
    {
        zmq_msg_init_data(&msg, msgs[i].data, msgs[i].size, zmq_free, NULL);
        rc = zmq_msg_send(&msg, sndr->sock, (i == count - 1) ? 0 : ZMQ_SNDMORE);
        if (rc != msgs[i].size)
        {
            CHUCHO_C_ERROR("yella.router",
                           "Could not send message (%zu): %s",
                           i,
                           zmq_strerror(zmq_errno()));
            zmq_msg_close(&msg);
            return false;
        }
    }
    return true;
}

void yella_set_router_message_received_callback(yella_router* rtr,
                                                yella_router_message_received_callback cb,
                                                void* data)
{
    yella_lock_mutex(rtr->mtx);
    rtr->recv_callback = cb;
    rtr->recv_callback_data = data;
    yella_unlock_mutex(rtr->mtx);
}

void yella_set_router_state_callback(yella_router* rtr, yella_router_state_callback cb, void* data)
{
    yella_lock_mutex(rtr->mtx);
    rtr->state_callback = cb;
    rtr->state_callback_data = data;
    yella_unlock_mutex(rtr->mtx);
}