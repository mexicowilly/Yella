#ifndef YELLA_PROCESS_H__
#define YELLA_PROCESS_H__

#include "export.h"
#include <unicode/utypes.h>
#include <stdio.h>

typedef struct yella_process yella_process;

YELLA_EXPORT yella_process* yella_create_process(const UChar* const command);
YELLA_EXPORT void yella_destroy_process(yella_process* proc);
YELLA_EXPORT FILE* yella_process_get_reader(yella_process* proc);

#endif
