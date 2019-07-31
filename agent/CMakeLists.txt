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

SET(YELLA_GEN_DIR "${CMAKE_BINARY_DIR}/gen/agent")
FILE(MAKE_DIRECTORY "${YELLA_GEN_DIR}")

INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}"
                    "${YELLA_CHUCHO_INCLUDE_DIR}"
                    "${YELLA_GEN_DIR}")
IF(YELLA_MACINTOSH)
    INCLUDE_DIRECTORIES(SYSTEM "${YELLA_FLATCC_INCLUDE_DIR}")
ENDIF()

OPTION(YELLA_PRIV_EXPORT "Whether private functions are visible" OFF)
IF(YELLA_PRIV_EXPORT)
    ADD_DEFINITIONS(-DYELLA_PRIV_EXPORT=YELLA_EXPORT)
ENDIF()

SET(YELLA_COMMON_GEN
    "${YELLA_GEN_DIR}/flatbuffers_common_builder.h"
    "${YELLA_GEN_DIR}/flatbuffers_common_reader.h")
ADD_CUSTOM_COMMAND(OUTPUT ${YELLA_COMMON_GEN}
                   COMMAND "${YELLA_FLATCC}" -c -w -o "${YELLA_GEN_DIR}")
ADD_CUSTOM_TARGET(flatb-common-gen
                  DEPENDS ${YELLA_COMMON_GEN})
MACRO(YELLA_GEN_TARGET YELLA_TARGET)
    UNSET(YELLA_TARGET_GEN)
    UNSET(YELLA_FLATCC_I_FLAGS)
    FOREACH(INC ${YELLA_FLATCC_INCLUDE_DIRS})
        LIST(APPEND YELLA_FLATCC_I_FLAGS "-I${CMAKE_SOURCE_DIR}/${INC}")
    ENDFOREACH()
    FOREACH(YELLA_FBS ${ARGN})
        GET_FILENAME_COMPONENT(YELLA_FBS_REAL "${YELLA_FBS}" REALPATH)
        GET_FILENAME_COMPONENT(YELLA_FBS_BASE "${YELLA_FBS_REAL}" NAME_WE)
        SET(YELLA_LOCAL_GEN
            "${YELLA_GEN_DIR}/${YELLA_FBS_BASE}_builder.h"
            "${YELLA_GEN_DIR}/${YELLA_FBS_BASE}_reader.h"
            "${YELLA_GEN_DIR}/${YELLA_FBS_BASE}_json_parser.h"
            "${YELLA_GEN_DIR}/${YELLA_FBS_BASE}_json_printer.h")
        ADD_CUSTOM_COMMAND(OUTPUT ${YELLA_LOCAL_GEN}
                           COMMAND "${YELLA_FLATCC}" ${YELLA_FLATCC_I_FLAGS} --json -w -o "${YELLA_GEN_DIR}" "${YELLA_FBS_REAL}"
                           DEPENDS "${YELLA_FBS_REAL}")
        LIST(APPEND YELLA_TARGET_GEN ${YELLA_LOCAL_GEN})
    ENDFOREACH()
    ADD_CUSTOM_TARGET(${YELLA_TARGET}-gen
                      DEPENDS ${YELLA_TARGET_GEN})
    IF(TARGET ${YELLA_TARGET})
        ADD_DEPENDENCIES(${YELLA_TARGET} flatb-common-gen ${YELLA_TARGET}-gen)
    ENDIF()
ENDMACRO()

ADD_SUBDIRECTORY(common)
ADD_SUBDIRECTORY(plugin)
ADD_SUBDIRECTORY(agent)
ADD_SUBDIRECTORY(test)
