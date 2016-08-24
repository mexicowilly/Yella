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

CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)

PROJECT(yella C)

INCLUDE(Configure.cmake)

ENABLE_TESTING()

SET(YELLA_VERSION 1.1)

SET(YELLA_GEN_DIR "${CMAKE_BINARY_DIR}/gen")
FILE(MAKE_DIRECTORY "${YELLA_GEN_DIR}")

INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}"
                    "${YELLA_CHUCHO_INCLUDE_DIR}"
                    "${YELLA_GEN_DIR}")

SET(YELLA_COMMON_GEN
    "${YELLA_GEN_DIR}/flatbuffers_common_builder.h"
    "${YELLA_GEN_DIR}/flatbuffers_common_reader.h")
ADD_CUSTOM_COMMAND(OUTPUT ${YELLA_COMMON_GEN}
                   COMMAND "${YELLA_FLATCC}" -c -w -o "${YELLA_GEN_DIR}")
ADD_CUSTOM_TARGET(flatb_common_gen
                  DEPENDS ${YELLA_COMMON_GEN})
MACRO(YELLA_GEN_TARGET YELLA_TARGET)
    UNSET(YELLA_TARGET_GEN)
    FOREACH(YELLA_FBS ${ARGN})
        GET_FILENAME_COMPONENT(YELLA_FBS_BASE "${YELLA_FBS}" NAME_WE)
        SET(YELLA_LOCAL_GEN
            "${YELLA_GEN_DIR}/${YELLA_FBS_BASE}_builder.h"
            "${YELLA_GEN_DIR}/${YELLA_FBS_BASE}_reader.h")
        ADD_CUSTOM_COMMAND(OUTPUT ${YELLA_LOCAL_GEN}
                           COMMAND "${YELLA_FLATCC}" -w -o "${YELLA_GEN_DIR}" "${YELLA_FBS}"
                           DEPENDS "${YELLA_FBS}")
        LIST(APPEND YELLA_TARGET_GEN ${YELLA_LOCAL_GEN})
    ENDFOREACH()
    ADD_CUSTOM_TARGET(${YELLA_TARGET}_gen
                      DEPENDS ${YELLA_TARGET_GEN})
    ADD_DEPENDENCIES(${YELLA_TARGET} flatb_common_gen ${YELLA_TARGET}_gen)
ENDMACRO()

ADD_SUBDIRECTORY(common)
ADD_SUBDIRECTORY(agent)
ADD_SUBDIRECTORY(test)
