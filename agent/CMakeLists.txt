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

INCLUDE_DIRECTORIES("${YELLA_ZEROMQ_INCLUDE_DIR}"
                    "${YELLA_FLATCC_INCLUDE_DIR}")

IF(YELLA_POSIX)
    INCLUDE_DIRECTORIES("${YELLA_UUID_INCLUDE_DIR}")
    SET(YELLA_PLATFORM_SOURCES
        platform/posix/runtime_link_posix.c
        platform/posix/yella_uuid_posix.c)
ENDIF()

IF(YELLA_LINUX)
    LIST(APPEND YELLA_PLATFORM_SOURCES
         platform/linux/mac_addresses_linux.c)
ENDIF()

ADD_EXECUTABLE(yella
               agent.c
               agent.h
               mac_addresses.h
               main.c
               router.c
               router.h
               saved_state.c
               saved_state.h
               spool.c
               spool.h
               yella_uuid.h
               ${YELLA_PLATFORM_SOURCES})

YELLA_GEN_TARGET(yella
                 serialization/private/saved_state.fbs)

TARGET_LINK_LIBRARIES(yella
                      common
                      "${YELLA_ZEROMQ_LIB}"
                      m)
IF(YELLA_POSIX)
    TARGET_LINK_LIBRARIES(yella "${YELLA_UUID_LIB}")
ENDIF()
IF(YELLA_LINUX)
    TARGET_LINK_LIBRARIES(yella dl)
ENDIF()

SET_SOURCE_FILES_PROPERTIES(main.c PROPERTIES
                            COMPILE_DEFINITIONS YELLA_VERSION=${YELLA_VERSION})
