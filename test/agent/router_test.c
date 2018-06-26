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

#include "agent/yella_uuid.h"
#include "agent/router.h"
#include "common/thread.h"
#include "common/settings.h"
#include <chucho/log.h>
#include <zmq.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

static const char* YELLA_MSG_TO_SEND = "My dog has fleas";

typedef struct test_state
{
    yella_event* server_is_ready;
    yella_event* receiver_is_ready;
    yella_thread* thr;
    yella_router* rtr;
    yella_uuid* id;
} test_state;

static void server_thread(void* p)
{
    void* ctx;
    void* sock;
    int rc;
    zmq_msg_t id_msg;
    zmq_msg_t delim_msg;
    zmq_msg_t payload_msg;
    zmq_msg_t header_msg;
    char* id;
    char* payload;
    test_state* arg;

    arg = (test_state*)p;
    ctx = zmq_ctx_new();
    sock = zmq_socket(ctx, ZMQ_ROUTER);
    assert_non_null(sock);
    rc = zmq_bind(sock, "tcp://*:19567");
    if (rc != 0)
    {
        CHUCHO_C_ERROR("router-test",
                       "zmq_bind: %s",
                       zmq_strerror(zmq_errno()));
        assert_true(false);
    }
    yella_signal_event(arg->server_is_ready);

    zmq_msg_init(&id_msg);
    rc = zmq_msg_recv(&id_msg, sock, 0);
    if (rc == -1)
    {
        CHUCHO_C_ERROR("router-test",
                       "zmq_msg_recv (id): %s",
                       zmq_strerror(zmq_errno()));
        assert_true(false);
    }
    id = malloc(zmq_msg_size(&id_msg) + 1);
    strncpy(id, (char*)zmq_msg_data(&id_msg), zmq_msg_size(&id_msg));
    zmq_msg_close(&id_msg);
    CHUCHO_C_INFO("router-test",
                  "Got identity: %s",
                  id);
    zmq_msg_init(&delim_msg);
    rc = zmq_msg_recv(&delim_msg, sock, 0);
    if (rc == -1)
    {
        CHUCHO_C_ERROR("router-test",
                       "zmq_msg_recv (delim): %s",
                       zmq_strerror(zmq_errno()));
        assert_true(false);
    }
    assert_int_equal(zmq_msg_size(&delim_msg), 0);
    zmq_msg_close(&delim_msg);
    CHUCHO_C_INFO("router-test",
                  "Got delimiter");
    zmq_msg_init(&payload_msg);
    rc = zmq_msg_recv(&payload_msg, sock, 0);
    if (rc == -1)
    {
        CHUCHO_C_ERROR("router-test",
                       "zmq_msg_recv (payload): %s",
                       zmq_strerror(zmq_errno()));
        assert_true(false);
    }
    payload = malloc(zmq_msg_size(&payload_msg) + 1);
    strncpy(payload, (char*)zmq_msg_data(&payload_msg), zmq_msg_size(&payload_msg) + 1);
    zmq_msg_close(&payload_msg);
    CHUCHO_C_INFO("router-test",
                  "Got payload: %s",
                  payload);
    assert_string_equal(payload, YELLA_MSG_TO_SEND);

    yella_wait_for_event(arg->receiver_is_ready);
    zmq_msg_init_size(&id_msg, strlen(id));
    memcpy(zmq_msg_data(&id_msg), id, strlen(id));
    free(id);
    rc = zmq_msg_send(&id_msg, sock, ZMQ_SNDMORE);
    if (rc == -1)
    {
        CHUCHO_C_ERROR("router-test",
                       "zmq_msg_send (id): %s",
                       zmq_strerror(zmq_errno()));
        assert_true(false);
    }
    zmq_msg_init(&delim_msg);
    rc = zmq_msg_send(&delim_msg, sock, ZMQ_SNDMORE);
    if (rc == -1)
    {
        CHUCHO_C_ERROR("router-test",
                       "zmq_msg_send (delim): %s",
                       zmq_strerror(zmq_errno()));
        assert_true(false);
    }
    zmq_msg_init_size(&header_msg, 1);
    *(char*)zmq_msg_data(&header_msg) = 'h';
    rc = zmq_msg_send(&header_msg, sock, ZMQ_SNDMORE);
    if (rc == -1)
    {
        CHUCHO_C_ERROR("router-test",
                       "zmq_msg_send (header): %s",
                       zmq_strerror(zmq_errno()));
        assert_true(false);
    }
    zmq_msg_init_size(&payload_msg, strlen(payload));
    memcpy(zmq_msg_data(&payload_msg), payload, strlen(id));
    free(payload);
    rc = zmq_msg_send(&payload_msg, sock, 0);
    if (rc == -1)
    {
        CHUCHO_C_ERROR("router-test",
                       "zmq_msg_send (payload): %s",
                       zmq_strerror(zmq_errno()));
        assert_true(false);
    }
    zmq_close(sock);
    zmq_ctx_term(ctx);
}

static void message_received(const yella_message_part* header,
                             const yella_message_part* body,
                             void* caller_data)
{
    assert_int_equal(1, header->size);
    assert_int_equal('h', *header->data);
    assert_int_equal(strlen(YELLA_MSG_TO_SEND), body->size);
    assert_true(memcmp(body->data, YELLA_MSG_TO_SEND, body->size) == 0);
    CHUCHO_C_INFO("router-test",
                  "Received message data back");
    yella_signal_event((yella_event*)caller_data);
}

static void receive(void** arg)
{
    yella_message_part msg;
    test_state* ts;
    yella_event* done_receiving;

    ts = (test_state*)*arg;
    done_receiving = yella_create_event();
    yella_set_router_message_received_callback(ts->rtr, message_received, done_receiving);
    yella_signal_event(ts->receiver_is_ready);
    yella_wait_for_event(done_receiving);
    yella_destroy_event(done_receiving);
}

static void send(void** arg)
{
    yella_message_part msg;
    test_state* ts;
    yella_sender* sndr;

    ts = (test_state*)*arg;
    yella_wait_for_event(ts->server_is_ready);
    msg.size = strlen(YELLA_MSG_TO_SEND);
    msg.data = malloc(msg.size);
    strncpy((char*)msg.data, YELLA_MSG_TO_SEND, msg.size);
    sndr = yella_create_sender(ts->rtr);
    yella_send(sndr, &msg, 1);
    yella_destroy_sender(sndr);
}


static void state_changed(yella_router_state st, void* data)
{
    if (st == YELLA_ROUTER_CONNECTED)
        yella_signal_event((yella_event*)data);
}

static int set_up(void** arg)
{
    test_state* targ;
    yella_event* state_event;

    *arg = malloc(sizeof(test_state));
    targ = *arg;
    targ->server_is_ready = yella_create_event();
    targ->receiver_is_ready = yella_create_event();
    targ->thr = yella_create_thread(server_thread, targ);
    yella_settings_set_text("router", "tcp://127.0.0.1:19567");
    yella_settings_set_uint("reconnect-timeout-seconds", 5);
    yella_settings_set_uint("poll-milliseconds", 500);
    targ->id = yella_create_uuid();
    targ->rtr = yella_create_router(targ->id);
    state_event = yella_create_event();
    yella_set_router_state_callback(targ->rtr, state_changed, state_event);
    yella_wait_for_event(state_event);
    yella_destroy_event(state_event);
    return 0;
}

static int tear_down(void** arg)
{
    test_state* targ;

    targ = *arg;
    yella_destroy_router(targ->rtr);
    yella_join_thread(targ->thr);
    yella_destroy_thread(targ->thr);
    yella_destroy_event(targ->server_is_ready);
    yella_destroy_event(targ->receiver_is_ready);
    yella_destroy_uuid(targ->id);
    free(targ);
    return 0;
}

int main()
{
    const struct CMUnitTest tests[] =
    {
        cmocka_unit_test(send),
        cmocka_unit_test(receive)
    };

#if defined(YELLA_POSIX)
    setenv("CMOCKA_TEST_ABORT", "1", 1);
#endif
    return cmocka_run_group_tests(tests, set_up, tear_down);
}
