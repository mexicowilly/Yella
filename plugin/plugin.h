#ifndef YELLA_PLUGIN_H__
#define YELLA_PLUGIN_H__

#include "common/return_code.h"
#include "common/message_header.h"
#include "plugin_reader.h"

typedef struct yella_agent_api
{
    void (*send_message)(void* agent, yella_message_header* mhdr, const uint8_t* const msg, size_t sz);
} yella_agent_api;

typedef struct yella_plugin_in_cap
{
    char* name;
    int version;
    yella_rc (*handler)(const yella_message_header* const mhdr, const uint8_t* const msg, size_t sz);
    char** configs;
    size_t config_count;
} yella_plugin_in_cap;

typedef struct yella_plugin_out_cap
{
    char* name;
    int version;
} yella_plugin_out_cap;

typedef struct yella_plugin
{
    char* name;
    char* version;
    yella_plugin_in_cap* in_caps;
    size_t in_cap_count;
    yella_plugin_out_cap* out_caps;
    size_t out_cap_count;
} yella_plugin;

YELLA_EXPORT yella_plugin* yella_copy_plugin(const yella_plugin* const plug);
YELLA_EXPORT void yella_destroy_plugin(yella_plugin* plug);
YELLA_EXPORT void yella_log_plugin_config(const char* const lgr, yella_fb_plugin_config_table_t cfg);

typedef const yella_plugin* (*plugin_start_func)(const yella_agent_api* api, void* agent);
typedef yella_rc (*plugin_stop_func)(void);
typedef const yella_plugin* (*plugin_status_func)(void);

#endif
