#ifndef YELLA_PLUGIN_H__
#define YELLA_PLUGIN_H__

#include "common/return_code.h"
#include "common/message_header.h"
#include "common/ptr_vector.h"
#include "common/sds.h"
#include "plugin_reader.h"
#include <chucho/logger.h>

typedef struct yella_agent_api
{
    void (*send_message)(void* agent, yella_message_header* mhdr, const uint8_t* const msg, size_t sz);
} yella_agent_api;

typedef yella_rc (*yella_in_cap_handler)(const yella_message_header* const mhdr, const uint8_t* const msg, size_t sz);

typedef struct yella_plugin_in_cap
{
    sds name;
    int version;
    yella_in_cap_handler handler;
    /* The vector of configs is of type sds */
    yella_ptr_vector* configs;
} yella_plugin_in_cap;

typedef struct yella_plugin_out_cap
{
    sds name;
    int version;
} yella_plugin_out_cap;

typedef struct yella_plugin
{
    sds name;
    sds version;
    yella_ptr_vector* in_caps;
    yella_ptr_vector* out_caps;
} yella_plugin;

YELLA_EXPORT yella_plugin* yella_copy_plugin(const yella_plugin* const plug);
YELLA_EXPORT yella_plugin* yella_create_plugin(const char* const name, const char* const version);
YELLA_EXPORT yella_plugin_in_cap* yella_create_plugin_in_cap(const char* const name, int version, yella_in_cap_handler handler);
YELLA_EXPORT yella_plugin_out_cap* yella_create_plugin_out_cap(const char* const name, int version);
YELLA_EXPORT void yella_destroy_plugin(yella_plugin* plug);
YELLA_EXPORT void yella_log_plugin_config(chucho_logger_t* lgr, yella_fb_plugin_config_table_t cfg);

typedef yella_plugin* (*yella_plugin_start_func)(const yella_agent_api* api, void* agent);
typedef yella_rc (*yella_plugin_stop_func)(void);
typedef yella_plugin* (*yella_plugin_status_func)(void);

#endif
