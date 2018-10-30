#ifndef YELLA_KAFKA_H__
#define YELLA_KAFKA_H__

#include "export.h"

typedef struct kafka kafka;

YELLA_EXPORT kafka* create_kafka(void);
YELLA_EXPORT void destroy_kafka(kafka* kf);

#endif
