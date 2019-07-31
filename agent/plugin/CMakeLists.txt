SET(PLUGIN_SO_VERSION 1)

ADD_LIBRARY(plugin SHARED
            plugin.c
            plugin.h)
SET_TARGET_PROPERTIES(plugin PROPERTIES
                      VERSION ${YELLA_VERSION}
                      SOVERSION ${PLUGIN_SO_VERSION}
                      COMPILE_DEFINITIONS yella_EXPORTS)
IF(DEFINED YELLA_SO_FLAGS)
    SET_TARGET_PROPERTIES(plugin PROPERTIES
                          COMPILE_FLAGS "${YELLA_SO_FLAGS}")
ENDIF()
TARGET_LINK_LIBRARIES(plugin common)

YELLA_GEN_TARGET(plugin
                 serialization/public/plugin.fbs)

SET(YELLA_FLATCC_INCLUDE_DIRS agent/plugin/serialization/public)

#ADD_SUBDIRECTORY(hello)
ADD_SUBDIRECTORY(file)
