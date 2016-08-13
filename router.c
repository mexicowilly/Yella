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
#include <stdbool.h>

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
    zmq_msg_init(&frame2);
    rc = zmq_msg_recv(&frame1, sock, 0);
    if (rc == -1)
    {
        CHUCHO_C_ERROR(yella_logger("yella.socket-monitor"),
                       "Unable to receive socket monitor event (frame 1): %s",
                       zmq_strerror(zmq_errno()));
        return false;
    }
    rc = zmq_msg_recv(&frame2, sock, 0);
    if (rc == -1)
    {
        CHUCHO_C_ERROR(yella_logger("yella.socket-monitor"),
                       "Unable to receive socket monitor event (frame 2): %s",
                       zmq_strerror(zmq_errno()));
        return false;
    }
    evt->id = *(uint16_t*)zmq_msg_data(&frame1);
    evt->value = *(uint32_t*)(zmq_msg_data(&frame1) + sizeof(uint16_t));
    strncpy(evt->endpoint, zmq_msg_data(&frame2), sizeof(evt->endpoint));
    if (zmq_msg_size(&frame2) > sizeof(evt->endpoint))
        evt->endpoint[sizeof(evt->endpoint) - 1] = 0;
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
    is_closed = false;
    while (!is_closed && read_monitor_event_msg(sock, &evt))
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
                          "Connection to %s will be retried in %u seconds",
                          evt.endpoint,
                          evt.value);
            set_state(rtr, YELLA_ROUTER_CONNECTION_PENDING);
            break;
        case ZMQ_EVENT_CLOSED:
            is_closed = true;
            set_state(rtr, YELLA_ROUTER_SOCKET_CLOSED);
            break;
        case ZMQ_EVENT_DISCONNECTED:
            CHUCHO_C_INFO(yella_logger("yella.socket-monitor"),
                          "Disconnected from %s",
                          evt.endpoint);
            break;
        }
    }
    zmq_close(sock);
    CHUCHO_C_INFO(yella_logger("yella.socket-monitor"),
                  "The socket monitor is stopping");
}

static void yella_zmq_free(void* data, void* part_data)
{
    free(data);
    free(part_data);
}

yella_router* yella_create_router(yella_uuid* id)
{
    int rc;
    const char* ep;
    yella_router* rtr;
    yella_monitor_thread_arg thrarg;
    yella_thread* thr;
    char* inproc;

    rtr = malloc(sizeof(yella_router));
    rtr->state = YELLA_ROUTER_CONNECTION_PENDING;
    rtr->mtx = NULL;
    rtr->context = zmq_ctx_new();
    rtr->socket = zmq_socket(rtr->context, ZMQ_DEALER);
    if (rtr->socket == NULL)
    {
        CHUCHO_C_ERROR(yella_logger("yella"),
                       "No sockets could be created");
        goto err_out;
    }
    ep = yella_settings_get_text("router");
    if (ep == NULL)
    {
        CHUCHO_C_ERROR(yella_logger("yella"),
                       "No router has been defined in settings");
        goto err_out;
    }
    inproc = create_monitor_socket_name(id);
    zmq_socket_monitor(rtr->socket, inproc, ZMQ_EVENT_ALL);
    thrarg.rtr = rtr;
    thrarg.inproc_name = inproc;
    thrarg.evt = yella_create_event();
    thr = yella_create_thread(yella_monitor_socket, &thrarg);
    yella_detach_thread(thr);
    yella_destroy_thread(thr);
    yella_wait_for_event(thrarg.evt);
    yella_destroy_event(thrarg.evt);
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

void yella_router_send(yella_router* rtr, yella_msg_part* msgs, size_t count)
{
    zmq_msg_t msg;
    size_t i;
    int rc;
    size_t expected;

    for (i = 0; i < count; i++)
    {
        zmq_msg_init_data(&msg, msgs[i].msg, msgs[i].size, yella_zmq_free, &msgs[i]);
        expected = msgs[i].size;
        yella_lock_mutex(rtr->mtx);
        rc = zmq_msg_send(&msg, rtr->socket, (i == count - 1) ? 0 : ZMQ_SNDMORE);
        yella_unlock_mutex(rtr->mtx);
        if (rc != expected)
        {
            CHUCHO_C_ERROR(yella_logger("yella"),
                           "Could not send message: %s",
                           zmq_strerror(rc));
            zmq_msg_close(&msg);
            break;
        }
    }
    /* Release unsent message data */
    for ( ; i < count; i++)
    {
        free(msgs[i].msg);
        free(&msgs[i]);
    }
    free(msgs);
}
