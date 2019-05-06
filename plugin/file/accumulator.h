#ifndef YELLA_ACCUMULATOR_H__
#define YELLA_ACCUMULATOR_H__

#include "plugin/plugin.h"
#include "plugin/file/element.h"
#include "file_reader.h"

typedef struct accumulator accumulator;

YELLA_PRIV_EXPORT void add_accumulator_message(accumulator* acc,
                                               const UChar* const recipient,
                                               const UChar* const config_name,
                                               const UChar* const elem_name,
                                               const element* const elem,
                                               yella_fb_file_condition_enum_t cond);
YELLA_PRIV_EXPORT accumulator* create_accumulator(void* agent, yella_agent_api* api);
YELLA_PRIV_EXPORT void destroy_accumulator(accumulator* acc);

#endif
