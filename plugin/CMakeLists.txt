ADD_LIBRARY(plugin SHARED
            plugin.c
            plugin.h)
TARGET_LINK_LIBRARIES(plugin common)

YELLA_GEN_TARGET(plugin
                 serialization/public/plugin.fbs)

SET(YELLA_FLATCC_INCLUDE_DIRS plugin/serialization/public)

ADD_SUBDIRECTORY(hello)
ADD_SUBDIRECTORY(file)
