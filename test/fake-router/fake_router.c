#include "agent/signal_handler.h"
#include "common/settings.h"
#include "common/macro_util.h"
#include "common/text_util.h"
#include "common/message_header.h"
#include <zmq.h>
#include <chucho/configuration.h>
#include <chucho/finalize.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdatomic.h>

static atomic_bool should_stop = false;

static void handle_message(const yella_message_header* hdr,
                           const uint8_t* const bytes,
                           size_t sz,
                           chucho_logger_t* lgr)
{
    yella_log_mhdr(hdr, lgr);
}

static void retrieve_router_settings(void)
{
    yella_setting_desc descs[] =
    {
        { "port", YELLA_SETTING_VALUE_UINT }
    };
    yella_settings_set_uint("router", "port", 19567);
    yella_retrieve_settings("router", descs, YELLA_ARRAY_SIZE(descs));
}

static void term_handler(void* udata)
{
    should_stop = true;
}

int main(int argc, char* argv[])
{
    void* ctx;
    sds bind_addr;
    int rc;
    void* sock;
    chucho_logger_t* lgr;
    int mrc;
    zmq_msg_t id_msg;
    zmq_msg_t delim_msg;
    zmq_msg_t hdr_msg;
    zmq_msg_t payload_msg;
    sds id;
    zmq_pollitem_t pi;
    int poll_count;
    yella_message_header* mhdr;

    mrc = EXIT_SUCCESS;
    install_signal_handler();
    set_signal_termination_handler(term_handler, NULL);
    yella_initialize_settings();
    yella_settings_set_text("router", "config-file", "./router.yaml");
    chucho_cnf_set_file_name(yella_settings_get_text("router", "config-file"));
    lgr = chucho_get_logger("router");
    CHUCHO_C_INFO_L(lgr, "Starting");
    yella_load_settings_doc();
    retrieve_router_settings();
    yella_destroy_settings_doc();
    ctx = zmq_ctx_new();
    sock = zmq_socket(ctx, ZMQ_ROUTER);
    if (sock == NULL)
    {
        CHUCHO_C_ERROR_L(lgr, "Could not create socket: %s", zmq_strerror(zmq_errno()));
        mrc = EXIT_FAILURE;
    }
    else
    {
        bind_addr = sdscatprintf(sdsempty(), "tcp://*:%" PRIu64, *yella_settings_get_uint("router", "port"));
        rc = zmq_bind(sock, bind_addr);
        sdsfree(bind_addr);
        if (rc == 0)
        {
            pi.socket = sock;
            pi.events = ZMQ_POLLIN;
            while (!should_stop)
            {
                poll_count = zmq_poll(&pi, 1, 500);
                if (!should_stop && poll_count > 0 && (pi.revents & ZMQ_POLLIN))
                {
                    zmq_msg_init(&id_msg);
                    rc = zmq_msg_recv(&id_msg, sock, 0);
                    if (rc == -1)
                    {
                        CHUCHO_C_ERROR_L(lgr,
                                         "zmq_msg_recv (id): %s",
                                         zmq_strerror(zmq_errno()));
                        mrc = EXIT_FAILURE;
                        break;
                    }
                    id = sdsnewlen(zmq_msg_data(&id_msg), zmq_msg_size(&id_msg));
                    zmq_msg_close(&id_msg);
                    CHUCHO_C_INFO_L(lgr, "Got identity: %s", id);
                    sdsfree(id);
                    zmq_msg_init(&delim_msg);
                    rc = zmq_msg_recv(&delim_msg, sock, 0);
                    if (rc == -1)
                    {
                        CHUCHO_C_ERROR_L(lgr,
                                         "zmq_msg_recv (delim): %s",
                                         zmq_strerror(zmq_errno()));
                        mrc = EXIT_FAILURE;
                        break;
                    }
                    zmq_msg_close(&delim_msg);
                    CHUCHO_C_INFO_L(lgr, "Got delimiter");
                    zmq_msg_init(&hdr_msg);
                    rc = zmq_msg_recv(&hdr_msg, sock, 0);
                    if (rc == -1)
                    {
                        CHUCHO_C_ERROR_L(lgr,
                                         "zmq_msg_recv (hdr): %s",
                                         zmq_strerror(zmq_errno()));
                        mrc = EXIT_FAILURE;
                        break;
                    }
                    mhdr = yella_unpack_mhdr(zmq_msg_data(&hdr_msg));
                    zmq_msg_close(&hdr_msg);
                    if (mhdr == NULL)
                    {
                        CHUCHO_C_ERROR_L(lgr, "Invalid message header");
                        mrc = EXIT_FAILURE;
                        break;
                    }
                    zmq_msg_init(&payload_msg);
                    rc = zmq_msg_recv(&payload_msg, sock, 0);
                    if (rc == -1)
                    {
                        CHUCHO_C_ERROR_L(lgr,
                                         "zmq_msg_recv (payload): %s",
                                         zmq_strerror(zmq_errno()));
                        mrc = EXIT_FAILURE;
                        break;
                    }
                    handle_message(mhdr, zmq_msg_data(&payload_msg), zmq_msg_size(&payload_msg), lgr);
                    zmq_msg_close(&payload_msg);
                    yella_destroy_mhdr(mhdr);
                }
            }
        }
        else
        {
            CHUCHO_C_ERROR_L(lgr, "Could not bind: %s", zmq_strerror(zmq_errno()));
            mrc = EXIT_FAILURE;
        }
    }
    if (sock != NULL)
        zmq_close(sock);
    zmq_ctx_destroy(ctx);
    yella_destroy_settings();
    CHUCHO_C_INFO_L(lgr, "Exiting");
    chucho_release_logger(lgr);
    chucho_finalize();
    return mrc;
}
