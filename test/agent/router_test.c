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

#if defined(YELLA_POSIX)
#include "platform/posix/yella_uuid_posix.c"
#endif
#include "router.c"
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

static const char* YELLA_MSG_TO_SEND = "My dog has fleas";

typedef struct server_thread_arg
{
    const char* command;
    yella_event* evt;
} server_thread_arg;

static void server_thread(void* p)
{
    void* ctx;
    void* sock;
    int rc;
    zmq_msg_t id_msg;
    zmq_msg_t delim_msg;
    zmq_msg_t payload_msg;
    char* id;
    char* payload;
    server_thread_arg* arg;

    arg = (server_thread_arg*)p;
    ctx = zmq_ctx_new();
    sock = zmq_socket(ctx, ZMQ_ROUTER);
    assert_non_null(sock);
    rc = zmq_bind(sock, "tcp://*:19567");
    if (rc != 0)
    {
        CHUCHO_C_ERROR(yella_logger("router-test"),
                       "zmq_bind: %s",
                       zmq_strerror(zmq_errno()));
        assert_true(false);
    }
    yella_signal_event(arg->evt);
    if (strcmp(arg->command, "receive") == 0)
    {
        zmq_msg_init(&id_msg);
        rc = zmq_msg_recv(&id_msg, sock, 0);
        if (rc == -1)
        {
            CHUCHO_C_ERROR(yella_logger("router-test"),
                           "zmq_msg_recv (id): %s",
                           zmq_strerror(zmq_errno()));
            assert_true(false);
        }
        id = malloc(zmq_msg_size(&id_msg) + 1);
        strncpy(id, (char*)zmq_msg_data(&id_msg), zmq_msg_size(&id_msg));
        zmq_msg_close(&id_msg);
        CHUCHO_C_INFO(yella_logger("router-test"),
                      "Got identity: %s",
                      id);
        free(id);
        zmq_msg_init(&delim_msg);
        rc = zmq_msg_recv(&delim_msg, sock, 0);
        if (rc == -1)
        {
            CHUCHO_C_ERROR(yella_logger("router-test"),
                           "zmq_msg_recv (delim): %s",
                           zmq_strerror(zmq_errno()));
            assert_true(false);
        }
        assert_int_equal(zmq_msg_size(&delim_msg), 0);
        zmq_msg_close(&delim_msg);
        CHUCHO_C_INFO(yella_logger("router-test"),
                      "Got delimiter");
        zmq_msg_init(&payload_msg);
        rc = zmq_msg_recv(&payload_msg, sock, 0);
        if (rc == -1)
        {
            CHUCHO_C_ERROR(yella_logger("router-test"),
                           "zmq_msg_recv (payload): %s",
                           zmq_strerror(zmq_errno()));
            assert_true(false);
        }
        payload = malloc(zmq_msg_size(&payload_msg) + 1);
        strncpy(payload, (char*)zmq_msg_data(&payload_msg), zmq_msg_size(&payload_msg) + 1);
        zmq_msg_close(&payload_msg);
        CHUCHO_C_INFO(yella_logger("router-test"),
                      "Got payload: %s",
                      payload);
        assert_string_equal(payload, YELLA_MSG_TO_SEND);
        free(payload);
    }
    zmq_close(sock);
    zmq_ctx_destroy(ctx);
}

static void send(void** arg)
{
    yella_uuid* id;
    yella_router* rtr;
    yella_msg_part* msg;
    yella_thread* thr;
    server_thread_arg targ;

    targ.command = "receive";
    targ.evt = yella_create_event();
    thr = yella_create_thread(server_thread, &targ);
    yella_wait_for_event(targ.evt);
    yella_destroy_event(targ.evt);
    yella_settings_set_text("router", "tcp://127.0.0.1:19567");
    id = yella_create_uuid();
    rtr = yella_create_router(id);
    msg = malloc(sizeof(yella_msg_part));
    msg->size = strlen(YELLA_MSG_TO_SEND);
    msg->msg = malloc(msg->size);
    strncpy(msg->msg, YELLA_MSG_TO_SEND, msg->size);
    yella_router_send(rtr, msg, 1);
    yella_destroy_router(rtr);
    yella_destroy_uuid(id);
    yella_join_thread(thr);
    yella_destroy_thread(thr);
}

int main()
{
    const struct CMUnitTest tests[] = { cmocka_unit_test(send) };

#if defined(YELLA_POSIX)
    /*
    helper = "test/agent/router_test_helper";
    if (!yella_file_exists(helper))
        assert_true(false);
        */
    setenv("CMOCKA_TEST_ABORT", "1", 1);
#endif
    return cmocka_run_group_tests(tests, NULL, NULL);
}
