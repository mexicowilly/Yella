#ifndef YELLA_COLLECT_ATTRIBUTES_H__
#define YELLA_COLLECT_ATTRIBUTES_H__

#include "plugin/file/element.h"
#include <chucho/logger.h>

YELLA_PRIV_EXPORT element* collect_attributes(const UChar* const name,
                                              const attribute_type* const attr_types,
                                              size_t attr_type_count,
                                              chucho_logger_t* lgr);

#endif
