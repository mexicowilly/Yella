#include "plugin/file/accumulator.h"
#include "common/thread.h"
#include "common/settings.h"
#include "common/text_util.h"
#include "common/parcel.h"
#include "common/sglib.h"
#include "common/compression.h"
#include "file_builder.h"
#include <chucho/log.h>
#include <unicode/ucal.h>

typedef struct msg_node
{
    uds recipient;
    struct flatcc_builder bld;
    size_t count;
    char color;
    struct msg_node* left;
    struct msg_node* right;
} msg_node;

struct accumulator
{
    void* agent;
    yella_agent_api* api;
    yella_thread* worker;
    yella_mutex* guard;
    yella_condition_variable* cond;
    bool should_stop;
    msg_node* recipients;
    chucho_logger_t* lgr;
};

#define MSG_NODE_COMPARATOR(lhs, rhs) (u_strcmp(lhs->recipient, rhs->recipient))

SGLIB_DEFINE_RBTREE_PROTOTYPES(msg_node, left, right, color, MSG_NODE_COMPARATOR);
SGLIB_DEFINE_RBTREE_FUNCTIONS(msg_node, left, right, color, MSG_NODE_COMPARATOR);

static bool no_size_excess(msg_node* nodes, uint64_t max_sz)
{
    struct sglib_msg_node_iterator itor;
    msg_node* cur;

    for (cur = sglib_msg_node_it_init(&itor, nodes);
         cur != NULL;
         cur = sglib_msg_node_it_next(&itor))
    {
        if (flatcc_builder_get_buffer_size(&cur->bld) >= max_sz)
            return false;
    }
    return true;
}

static void worker_main(void* arg)
{
    accumulator* acc;
    UDate time_limit;
    size_t latency_millis;
    size_t millis_to_wait;
    uint64_t max_msg_sz;
    yella_parcel* pcl;
    uint32_t minor_seq;
    struct sglib_msg_node_iterator itor;
    msg_node* cur;
    uint8_t* packed;
    size_t packed_sz;
    bool time_limit_reached;
    char* utf8;

    acc = arg;
    CHUCHO_C_INFO(acc->lgr, "The accumulator thread is starting");
    minor_seq = 0;
    max_msg_sz = *yella_settings_get_byte_size(u"agent", u"max-message-size");
    latency_millis = *yella_settings_get_uint(u"file", u"send-latency-seconds") * 1000;
    while (true)
    {
        millis_to_wait = latency_millis;
        time_limit = ucal_getNow() + millis_to_wait;
        yella_lock_mutex(acc->guard);
        while (!acc->should_stop &&
               ucal_getNow() < time_limit &&
               no_size_excess(acc->recipients, max_msg_sz))
        {
            yella_wait_milliseconds_for_condition_variable(acc->cond, acc->guard, millis_to_wait);
            millis_to_wait = time_limit - ucal_getNow();
        }
        if (acc->should_stop)
        {
            yella_unlock_mutex(acc->guard);
            break;
        }
        time_limit_reached = ucal_getNow() >= time_limit;
        for (cur = sglib_msg_node_it_init(&itor, acc->recipients);
             cur != NULL;
             cur = sglib_msg_node_it_next(&itor))
        {
            if (cur->count > 0 && (time_limit_reached || flatcc_builder_get_buffer_size(&cur->bld) >= max_msg_sz))
            {
                pcl = yella_create_parcel(cur->recipient, u"yella.fb.file.file_states");
                pcl->cmp = YELLA_COMPRESSION_LZ4;
                pcl->seq.minor = minor_seq++;
                yella_fb_file_file_states_states_add(&cur->bld, yella_fb_file_file_state_vec_end(&cur->bld));
                yella_fb_file_file_states_end_as_root(&cur->bld);
                packed = flatcc_builder_finalize_buffer(&cur->bld, &pcl->payload_size);
                pcl->payload = yella_lz4_compress(packed, &pcl->payload_size);
                free(packed);
                acc->api->send_message(acc->agent, pcl);
                yella_destroy_parcel(pcl);
                flatcc_builder_reset(&cur->bld);
                yella_fb_file_file_states_start_as_root(&cur->bld);
                yella_fb_file_file_state_vec_start(&cur->bld);
                if (chucho_logger_permits(acc->lgr, CHUCHO_INFO))
                {
                    utf8 = yella_to_utf8(cur->recipient);
                    CHUCHO_C_INFO(acc->lgr, "Sent %zu events to recipient '%s'", cur->count, utf8);
                    free(utf8);
                }
                cur->count = 0;
            }
        }
        yella_unlock_mutex(acc->guard);
    }
    yella_lock_mutex(acc->guard);
    for (cur = sglib_msg_node_it_init(&itor, acc->recipients);
         cur != NULL;
         cur = sglib_msg_node_it_next(&itor))
    {
        if (cur->count > 0)
        {
            pcl = yella_create_parcel(cur->recipient, u"yella.fb.file.file_states");
            pcl->cmp = YELLA_COMPRESSION_LZ4;
            pcl->seq.minor = minor_seq++;
            yella_fb_file_file_states_states_add(&cur->bld, yella_fb_file_file_state_vec_end(&cur->bld));
            yella_fb_file_file_states_end_as_root(&cur->bld);
            packed = flatcc_builder_finalize_buffer(&cur->bld, &pcl->payload_size);
            pcl->payload = yella_lz4_compress(packed, &pcl->payload_size);
            free(packed);
            acc->api->send_message(acc->agent, pcl);
            yella_destroy_parcel(pcl);
            if (chucho_logger_permits(acc->lgr, CHUCHO_INFO))
            {
                utf8 = yella_to_utf8(cur->recipient);
                CHUCHO_C_INFO(acc->lgr, "Sent %zu events to recipient '%s'", cur->count, utf8);
                free(utf8);
            }
            cur->count = 0;
        }
        flatcc_builder_clear(&cur->bld);
    }
    yella_unlock_mutex(acc->guard);
    CHUCHO_C_INFO(acc->lgr, "The accumulator thread is ending");
}

size_t accumulator_size(accumulator* acc)
{
    struct sglib_msg_node_iterator itor;
    msg_node* cur;
    size_t result;

    result = 0;
    yella_lock_mutex(acc->guard);
    for (cur = sglib_msg_node_it_init(&itor, acc->recipients);
         cur != NULL;
         cur = sglib_msg_node_it_next(&itor))
    {
        result += flatcc_builder_get_buffer_size(&cur->bld);
    }
    yella_unlock_mutex(acc->guard);
    return result;
}

void add_accumulator_message(accumulator* acc,
                             const UChar* const recipient,
                             const UChar* const config_name,
                             const UChar* const elem_name,
                             const element* const elem,
                             yella_fb_file_condition_enum_t cond)
{
    char* utf8;
    msg_node to_find;
    msg_node* found;
    char* utf81;
    char* utf82;

    to_find.recipient = (UChar*)recipient;
    yella_lock_mutex(acc->guard);
    found = sglib_msg_node_find_member(acc->recipients, &to_find);
    if (found == NULL)
    {
        found = malloc(sizeof(msg_node));
        found->recipient = udsnew(recipient);
        flatcc_builder_init(&found->bld);
        yella_fb_file_file_states_start_as_root(&found->bld);
        yella_fb_file_file_state_vec_start(&found->bld);
        found->count = 0;
        sglib_msg_node_add(&acc->recipients, found);
    }
    yella_fb_file_file_state_start(&found->bld);
    yella_fb_file_file_state_milliseconds_since_epoch_add(&found->bld, ucal_getNow());
    utf8 = yella_to_utf8(config_name);
    yella_fb_file_file_state_config_name_create_str(&found->bld, utf8);
    free(utf8);
    yella_fb_file_file_state_cond_add(&found->bld, cond);
    utf8 = yella_to_utf8(elem_name);
    yella_fb_file_file_state_file_name_create_str(&found->bld, utf8);
    free(utf8);
    if (elem != NULL)
        yella_fb_file_file_state_attrs_add(&found->bld, pack_element_attributes_to_vector(elem, &found->bld));
    yella_fb_file_file_state_vec_push(&found->bld, yella_fb_file_file_state_end(&found->bld));
    ++found->count;
    if (chucho_logger_permits(acc->lgr, CHUCHO_DEBUG))
    {
        utf8 = yella_to_utf8(recipient);
        utf81 = yella_to_utf8(config_name);
        utf82 = yella_to_utf8(elem_name);
        CHUCHO_C_DEBUG(acc->lgr, "Added event for recipient '%s', config '%s', element '%s'", utf8, utf81, utf82);
        free(utf82);
        free(utf81);
        free(utf8);
    }
    yella_signal_condition_variable(acc->cond);
    yella_unlock_mutex(acc->guard);
}

accumulator* create_accumulator(void* agent, const yella_agent_api* const api)
{
    accumulator* result;

    result = malloc(sizeof(accumulator));
    result->lgr = chucho_get_logger("file.accumulator");
    result->agent = agent;
    result->api = malloc(sizeof(yella_agent_api));
    result->api->send_message = api->send_message;
    result->guard = yella_create_mutex();
    result->cond = yella_create_condition_variable();
    result->should_stop = false;
    result->recipients = NULL;
    result->worker = yella_create_thread(worker_main, result);
    return result;
}

void destroy_accumulator(accumulator* acc)
{
    struct sglib_msg_node_iterator itor;
    msg_node* cur;

    if (acc != NULL)
    {
        yella_lock_mutex(acc->guard);
        acc->should_stop = true;
        yella_signal_condition_variable(acc->cond);
        yella_unlock_mutex(acc->guard);
        yella_join_thread(acc->worker);
        yella_destroy_thread(acc->worker);
        for (cur = sglib_msg_node_it_init(&itor, acc->recipients);
             cur != NULL;
             cur = sglib_msg_node_it_next(&itor))
        {
            sglib_msg_node_delete(&acc->recipients, cur);
            udsfree(cur->recipient);
            free(cur);
        }
        yella_destroy_condition_variable(acc->cond);
        yella_destroy_mutex(acc->guard);
        free(acc->api);
        chucho_release_logger(acc->lgr);
        free(acc);
    }
}
