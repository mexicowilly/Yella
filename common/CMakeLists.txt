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

SET(COMMON_SO_VERSION 1)

INCLUDE_DIRECTORIES("${YELLA_LIBYAML_INCLUDE_DIR}"
                    "${YELLA_FLATCC_INCLUDE_DIR}")

IF(YELLA_POSIX)
    SET(COMMON_PLATFORM_SOURCES
        platform/posix/calendar_posix.c
        platform/posix/file_posix.c
        platform/posix/settings_posix.c
        platform/posix/text_util_posix.c
        platform/posix/thread_posix.c
        platform/posix/time_util_posix.c)
    SET_SOURCE_FILES_PROPERTIES(platform/posix/settings_posix.c
                                PROPERTIES
                                COMPILE_DEFINITIONS "YELLA_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX}")
ENDIF()

ADD_LIBRARY(common SHARED
            calendar.h
            compression.c
            compression.h
            file.c
            file.h
            macro_util.h
            message_header.c
            message_header.h
            message_part.h
            ptr_vector.c
            ptr_vector.h
            return_code.c
            return_code.h
            sds.c
            sds.h
            sdsalloc.h
            sds_util.c
            sds_util.h
            settings.c
            settings.h
            sglib.h
            text_util.h
            thread.h
            time_util.h
            ${COMMON_PLATFORM_SOURCES})
SET_TARGET_PROPERTIES(common PROPERTIES
                      VERSION ${YELLA_VERSION}
                      SOVERSION ${COMMON_SO_VERSION}
                      COMPILE_DEFINITIONS yella_EXPORTS)
IF(DEFINED YELLA_SO_FLAGS)
    SET_TARGET_PROPERTIES(common PROPERTIES
                          COMPILE_FLAGS "${YELLA_SO_FLAGS}")
ENDIF()
YELLA_GEN_TARGET(common
                 serialization/public/header.fbs)
TARGET_LINK_LIBRARIES(common
                      "${YELLA_FLATCC_LIB}"
                      "${YELLA_CHUCHO_LIB}"
                      "${YELLA_LIBYAML_LIB}"
                      "${YELLA_LZ4_LIB}"
                      Threads::Threads
                      ${YELLA_MEM_LIBS})
