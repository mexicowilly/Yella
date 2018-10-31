#ifndef YELLA_KAFKA_H__
#define YELLA_KAFKA_H__

#include "export.h"
#include "agent/yella_uuid.h"

typedef struct kafka kafka;

YELLA_EXPORT kafka* create_kafka(const yella_uuid* const id);
YELLA_EXPORT void destroy_kafka(kafka* kf);

#endif
