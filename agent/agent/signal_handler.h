#if !defined(YELLA_SIGNAL_HANDLER_H__)
#define YELLA_SIGNAL_HANDLER_H__

#include "export.h"

typedef struct sig_info
{
    int sig;
    char* name;
    char* description;
} sig_info;

YELLA_EXPORT void install_signal_handler(void);
YELLA_EXPORT void set_signal_termination_handler(void (*hndlr)(void*), void* udata);

#endif
