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

SET(YELLA_VERSION 1.1)

INCLUDE_DIRECTORIES("${CMAKE_SOURCE_DIR}"
                    "${YELLA_CHUCHO_INCLUDE_DIR}"
                    "${YELLA_ZEROMQ_INCLUDE_DIR}")
IF(YELLA_POSIX)
    INCLUDE_DIRECTORIES("${YELLA_UUID_INCLUDE_DIR}")
ENDIF()

ADD_SUBDIRECTORY(common)

IF(YELLA_POSIX)
    SET(YELLA_PLATFORM_SOURCES
        platform/posix/runtime_link.c
        platform/posix/yella_uuid.c)
ENDIF()

ADD_EXECUTABLE(yella
               main.c
               router.c
               router.h
               yella_uuid.h
               ${YELLA_PLATFORM_SOURCES})
TARGET_LINK_LIBRARIES(yella common
                      "${YELLA_ZEROMQ_LIB}")
IF(YELLA_POSIX)
    TARGET_LINK_LIBRARIES(yella "${YELLA_UUID_LIB}")
ENDIF()
IF(YELLA_LINUX)
    TARGET_LINK_LIBRARIES(yella dl)
ENDIF()

SET_SOURCE_FILES_PROPERTIES(main.c PROPERTIES
                            COMPILE_DEFINITIONS YELLA_VERSION=${YELLA_VERSION})

ENABLE_TESTING()
ADD_SUBDIRECTORY(test)
