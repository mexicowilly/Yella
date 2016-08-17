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
#include "common/log.h"
#include "common/thread.h"
#include <zmq.h>
#include <stdlib.h>
#include <assert.h>

typedef struct yella_router
{
    void* context;
    void* socket;
    yella_mutex* mtx;
    yella_router_state state;
} yella_router;

typedef struct yella_monitor_event
{
    uint16_t id;
    uint32_t value;
    char endpoint[1024];
} yella_monitor_event;

typedef struct yella_monitor_thread_arg
{
    yella_router* rtr;
    const char* inproc_name;
    yella_event* evt;
} yella_monitor_thread_arg;

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
        CHUCHO_C_INFO(yella_logger("yella.router"),
                      "The router state is changing: %s ==> %s",
                      state_text[rtr->state],
                      state_text[state]);
        rtr->state = state;
    }
    yella_unlock_mutex(rtr->mtx);
}

static char* create_monitor_socket_name(yella_uuid* id)
{
    const char* id_text;
    char* ep;

    id_text = yella_uuid_to_text(id);
    ep = malloc(9 + strlen(id_text) + 1);
    strcpy(ep, "inproc://");
    strcat(ep, id_text);
    return ep;
}

static bool read_monitor_event_msg(void* sock, yella_monitor_event* evt)
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
    strncpy(evt->endpoint, zmq_msg_data(&frame2), sizeof(evt->endpoint));
    if (zmq_msg_size(&frame2) > sizeof(evt->endpoint))
        evt->endpoint[sizeof(evt->endpoint) - 1] = 0;
    zmq_msg_close(&frame2);
    return true;
}

static void yella_monitor_socket(void* p)
{
    yella_monitor_event evt;
    void* sock;
    yella_monitor_thread_arg* arg;
    int rc;
    bool is_closed;
    yella_router* rtr;

    CHUCHO_C_INFO(yella_logger("yella.socket-monitor"),
                  "Starting socket monitor");
    arg = (yella_monitor_thread_arg*)p;
    rtr = arg->rtr;
    sock = zmq_socket(rtr->context, ZMQ_PAIR);
    if (sock == NULL)
    {
        CHUCHO_C_ERROR(yella_logger("yella.socket-monitor"),
                       "Unable to create the socket monitor socket",
                       zmq_strerror(zmq_errno()));
        return;
    }
    rc = zmq_connect(sock, arg->inproc_name);
    if (rc != 0)
    {
        CHUCHO_C_ERROR(yella_logger("yella.socket-monitor"),
                       "Unable to connect to %s: %s",
                       arg->inproc_name,
                       zmq_strerror(zmq_errno()));
        zmq_close(sock);
        return;
    }
    yella_signal_event(arg->evt);
    /* The arg variable must not be used from here on */
    while (read_monitor_event_msg(sock, &evt))
    {
        switch (evt.id)
        {
        case ZMQ_EVENT_CONNECTED:
            CHUCHO_C_INFO(yella_logger("yella.socket-monitor"),
                          "Completed connection to %s",
                          evt.endpoint);
            set_state(rtr, YELLA_ROUTER_CONNECTED);
            break;
        case ZMQ_EVENT_CONNECT_DELAYED:
            CHUCHO_C_INFO(yella_logger("yella.socket-monitor"),
                          "Connection to %s delayed",
                          evt.endpoint);
            set_state(rtr, YELLA_ROUTER_CONNECTION_PENDING);
            break;
        case ZMQ_EVENT_CONNECT_RETRIED:
            CHUCHO_C_INFO(yella_logger("yella.socket-monitor"),
                          "Connection to %s will be retried in %u milliseconds",
                          evt.endpoint,
                          evt.value);
            set_state(rtr, YELLA_ROUTER_CONNECTION_PENDING);
            break;
        case ZMQ_EVENT_CLOSED:
            set_state(rtr, YELLA_ROUTER_SOCKET_CLOSED);
            break;
        case ZMQ_EVENT_DISCONNECTED:
            CHUCHO_C_INFO(yella_logger("yella.socket-monitor"),
                          "Disconnected from %s",
                          evt.endpoint);
            set_state(rtr, YELLA_ROUTER_DISCONNECTED);
            break;
        }
    }
    zmq_close(sock);
    CHUCHO_C_INFO(yella_logger("yella.socket-monitor"),
                  "The socket monitor is stopping");
}

static void yella_zmq_free(void* data, void* a)
{
    free(data);
}

yella_router* yella_create_router(yella_uuid* id)
{
    int rc;
    const char* ep;
    yella_router* rtr;
    yella_monitor_thread_arg thrarg;
    yella_thread* thr;
    char* inproc;
    const char* id_text;

    ep = yella_settings_get_text("router");
    if (ep == NULL)
    {
        CHUCHO_C_ERROR(yella_logger("yella.router"),
                       "No router has been defined in settings");
        return NULL;
    }
    rtr = malloc(sizeof(yella_router));
    rtr->state = YELLA_ROUTER_CONNECTION_PENDING;
    rtr->mtx = NULL;
    rtr->context = zmq_ctx_new();
    rtr->socket = zmq_socket(rtr->context, ZMQ_DEALER);
    if (rtr->socket == NULL)
    {
        CHUCHO_C_ERROR(yella_logger("yella.router"),
                       "No sockets could be created");
        goto err_out;
    }
    id_text = yella_uuid_to_text(id);
    zmq_setsockopt(rtr->socket,
                   ZMQ_IDENTITY,
                   id_text,
                   strlen(id_text));
    inproc = create_monitor_socket_name(id);
    rc = zmq_socket_monitor(rtr->socket, inproc, ZMQ_EVENT_ALL);
    if (rc == -1)
    {
        CHUCHO_C_ERROR(yella_logger("yella.router"),
                       "Could not create socket monitor at %s: %s",
                       inproc,
                       zmq_strerror(zmq_errno()));
    }
    else
    {
        thrarg.rtr = rtr;
        thrarg.inproc_name = inproc;
        thrarg.evt = yella_create_event();
        thr = yella_create_thread(yella_monitor_socket, &thrarg);
        yella_detach_thread(thr);
        yella_destroy_thread(thr);
        yella_wait_for_event(thrarg.evt);
        yella_destroy_event(thrarg.evt);
    }
    free(inproc);
    rc = zmq_connect(rtr->socket, ep);
    if (rc != 0)
    {
        CHUCHO_C_ERROR(yella_logger("yella"),
                       "Could not connect to %s: %s",
                       ep,
                       zmq_strerror(rc));
        goto err_out;
    }
    rtr->mtx = yella_create_mutex();
    return rtr;
err_out:
    yella_destroy_router(rtr);
    return NULL;

}

void yella_destroy_router(yella_router* rtr)
{
    if (rtr->mtx != NULL)
        yella_destroy_mutex(rtr->mtx);
    if (rtr->socket != NULL)
        zmq_close(rtr->socket);
    if (rtr->context != NULL)
        zmq_ctx_destroy(rtr->context);
    free(rtr);
}

void yella_router_receive(yella_router* rtr,
                          yella_msg_part* part,
                          size_t count,
                          size_t milliseconds_to_wait)
{
    zmq_pollitem_t pi;
    int rc;
    zmq_msg_t delim;
    zmq_msg_t payload;
    size_t i;

    for (i = 0; i < count; i++)
    {
        part[i].data = NULL;
        part[i].size = 0;
    }
    pi.socket = rtr->socket;
    pi.events = ZMQ_POLLIN;
    if (zmq_poll(&pi, 1, milliseconds_to_wait) > 0)
    {
        zmq_msg_init(&delim);
        rc = zmq_msg_recv(&delim, rtr->socket, 0);
        if (rc == -1)
        {
            CHUCHO_C_ERROR(yella_logger("yella"),
                           "Could not send message delimiter: %s",
                           zmq_strerror(zmq_errno()));
        }
        assert(zmq_msg_size(&delim) == 0);
        zmq_msg_close(&delim);
        for (i = 0; i < count; i++)
        {
            if (zmq_poll(&pi, 1, milliseconds_to_wait) > 0)
            {
                zmq_msg_init(&payload);
                rc = zmq_msg_recv(&payload, rtr->socket, 0);
                if (zmq_msg_size(&payload) > 0)
                {
                    part[i].size = zmq_msg_size(&payload);
                    part[i].data = malloc(part[i].size);
                    memcpy(part[i].data, zmq_msg_data(&payload), part[i].size);
                }
                zmq_msg_close(&payload);
            }
            else
            {
                break;
            }
        }
    }
}

bool yella_router_send(yella_router* rtr, yella_msg_part* msgs, size_t count)
{
    zmq_msg_t msg;
    size_t i;
    int rc;
    size_t expected;
    bool result;

    result = true;
    zmq_msg_init(&msg);
    rc = zmq_msg_send(&msg, rtr->socket, ZMQ_SNDMORE);
    if (rc == -1)
    {
        CHUCHO_C_ERROR(yella_logger("yella"),
                       "Could not send message delimiter: %s",
                       zmq_strerror(zmq_errno()));
        zmq_msg_close(&msg);
        i = 0;
        result = false;
    }
    else
    {
        for (i = 0; i < count; i++)
        {
            zmq_msg_init_data(&msg, msgs[i].data, msgs[i].size, yella_zmq_free, NULL);
            expected = msgs[i].size;
            yella_lock_mutex(rtr->mtx);
            rc = zmq_msg_send(&msg, rtr->socket, (i == count - 1) ? 0 : ZMQ_SNDMORE);
            yella_unlock_mutex(rtr->mtx);
            if (rc != expected)
            {
                CHUCHO_C_ERROR(yella_logger("yella"),
                               "Could not send message (%i): %s",
                               i,
                               zmq_strerror(zmq_errno()));
                zmq_msg_close(&msg);
                result = false;
                break;
            }
        }
    }
    /* Release unsent message data */
    for ( ; i < count; i++)
        free(msgs[i].data);
    return result;
}
