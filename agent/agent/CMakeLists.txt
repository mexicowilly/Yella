#
# Copyright 2016 Will Mason
# 
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#      http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

SET(AGENT_SO_VERSION 1)

INCLUDE_DIRECTORIES("${YELLA_FLATCC_INCLUDE_DIR}")

SET(YELLA_SOURCES
    agent.c
    agent.h
    argparse.c
    argparse.h
    heartbeat.c
    heartbeat.h
    mac_addresses.c
    mac_addresses.h
    router.c
    router.h
    runtime_link.h
    saved_state.c
    saved_state.h
    signal_handler.h
    spool.c
    spool.h
    yella_uuid.h)

IF(YELLA_POSIX)
    INCLUDE_DIRECTORIES("${YELLA_UUID_INCLUDE_DIR}")
    LIST(APPEND YELLA_SOURCES
         platform/posix/heartbeat_posix.c
         platform/posix/runtime_link_posix.c
         platform/posix/signal_handler_posix.c
         platform/posix/yella_uuid_posix.c)
ENDIF()

IF(YELLA_FREEBSD)
    LIST(APPEND YELLA_SOURCES
         platform/freebsd/mac_addresses_freebsd.c)
ENDIF()

IF(YELLA_LINUX)
    LIST(APPEND YELLA_SOURCES
         platform/linux/mac_addresses_linux.c)
ENDIF()

IF(YELLA_MACINTOSH)
    LIST(APPEND YELLA_SOURCES
         platform/macintosh/mac_addresses_macintosh.c)
ENDIF()

ADD_LIBRARY(agent SHARED ${YELLA_SOURCES})
SET_TARGET_PROPERTIES(agent PROPERTIES
                      VERSION ${YELLA_VERSION}
                      SOVERSION ${AGENT_SO_VERSION}
                      COMPILE_DEFINITIONS yella_EXPORTS)
IF(DEFINED YELLA_SO_FLAGS)
    SET_TARGET_PROPERTIES(agent PROPERTIES
                          COMPILE_FLAGS "${YELLA_SO_FLAGS}")
ENDIF()
ADD_DEPENDENCIES(all-targets agent)

YELLA_GEN_TARGET(agent
                 serialization/public/heartbeat.fbs
                 serialization/private/saved_state.fbs)

TARGET_LINK_LIBRARIES(agent
                      plugin
                      "${YELLA_ZEROMQ_LIB}"
                      m)
IF(YELLA_MACINTOSH)
    TARGET_LINK_LIBRARIES(agent "-framework IOKit" "-framework CoreFoundation")
ENDIF()
IF(YELLA_POSIX)
    TARGET_LINK_LIBRARIES(agent "${YELLA_UUID_LIB}")
ENDIF()
IF(YELLA_LINUX)
    TARGET_LINK_LIBRARIES(agent dl)
ENDIF()

ADD_EXECUTABLE(yellad main.c)
TARGET_LINK_LIBRARIES(yellad agent)
ADD_DEPENDENCIES(all-targets yellad)

SET_SOURCE_FILES_PROPERTIES(main.c PROPERTIES
                            COMPILE_DEFINITIONS YELLA_VERSION=${YELLA_VERSION})
