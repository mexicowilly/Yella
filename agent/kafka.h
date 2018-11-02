#ifndef YELLA_KAFKA_H__
#define YELLA_KAFKA_H__

#include "export.h"
#include "agent/yella_uuid.h"
#include <stdbool.h>

typedef struct kafka kafka;

typedef void (*kafka_message_handler)(void* msg, size_t len);

YELLA_EXPORT kafka* create_kafka(const yella_uuid* const id);
YELLA_EXPORT void destroy_kafka(kafka* kf);
YELLA_EXPORT bool send_kafka_message(kafka* kf, const UChar* const tpc, void* msg, size_t len);
YELLA_EXPORT void set_kafka_message_handler(kafka* kf, kafka_message_handler hndlr);

#endif
