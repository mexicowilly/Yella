#ifndef YELLA_PLUGIN_H__
#define YELLA_PLUGIN_H__

#include "common/return_code.h"
#include "common/ptr_vector.h"
#include "common/uds.h"
#include "common/message_header.h"
#include "common/message_part.h"
#include "plugin_reader.h"
#include <chucho/logger.h>

typedef struct yella_agent_api
{
    /* mhdr is owned by the caller, but the agent needs to mutate it. The msg is owned by the callee. */
    void (*send_message)(void* agent, yella_message_header* mhdr, uint8_t* msg, size_t sz);
} yella_agent_api;

typedef yella_rc (*yella_in_cap_handler)(const yella_message_header* const mhdr, const yella_message_part* const msg, void* udata);

typedef struct yella_plugin_in_cap
{
    uds name;
    int version;
    yella_in_cap_handler handler;
    void* udata;
    /* The vector of configs is of type uds */
    yella_ptr_vector* configs;
} yella_plugin_in_cap;

typedef struct yella_plugin_out_cap
{
    uds name;
    int version;
} yella_plugin_out_cap;

typedef struct yella_plugin
{
    uds name;
    uds version;
    yella_ptr_vector* in_caps;
    yella_ptr_vector* out_caps;
    void* udata;
} yella_plugin;

YELLA_EXPORT yella_plugin* yella_copy_plugin(const yella_plugin* const plug);
YELLA_EXPORT yella_plugin* yella_create_plugin(const UChar* const name, const UChar* const version, void* udata);
YELLA_EXPORT yella_plugin_in_cap* yella_create_plugin_in_cap(const UChar* const name, int version, yella_in_cap_handler handler, void* udata);
YELLA_EXPORT yella_plugin_out_cap* yella_create_plugin_out_cap(const UChar* const name, int version);
YELLA_EXPORT void yella_destroy_plugin(yella_plugin* plug);
YELLA_EXPORT void yella_log_plugin_config(chucho_logger_t* lgr, yella_fb_plugin_config_table_t cfg);

typedef yella_plugin* (*yella_plugin_start_func)(const yella_agent_api* api, void* agent);
typedef yella_rc (*yella_plugin_stop_func)(void* udata);
typedef yella_plugin* (*yella_plugin_status_func)(void* udata);

#endif
