#include "common/calendar.h"
#include <stdlib.h>
#include <stdbool.h>

char* yella_format_time(const char * const fmt, time_t t)
{
    struct tm pieces;
    size_t count;
    char* result;
    size_t rc;

    gmtime_r(&t, &pieces);
    count = 100;
    result = malloc(count);
    rc = 0;
    while (true)
    {
        rc = strftime(result, count, fmt, &pieces);
        if (rc != 0)
        {
            break;
        }
        else
        {
            count *= 2;
            result = realloc(result, count);
        }
    }
    result = realloc(result, rc + 1);
    return result;
}
