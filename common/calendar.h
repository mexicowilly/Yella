#if !defined(YELLA_CALENDAR_H__)
#define YELLA_CALENDAR_H__

#include "export.h"
#include <time.h>

YELLA_EXPORT char* yella_format_time(const char* const fmt, time_t t);

#endif
