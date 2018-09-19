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

INCLUDE_DIRECTORIES("${YELLA_CMOCKA_INCLUDE_DIR}")

IF(YELLA_POSIX)
    ADD_DEFINITIONS(-DYELLA_POSIX)
ENDIF()

ADD_CUSTOM_TARGET(no-check)

MACRO(YELLA_TEST YELLA_TEST_NAME)
    STRING(REPLACE "-" "_" YELLA_TEST_SRC "${YELLA_TEST_NAME}")
    SET(YELLA_TEST_SRC "${YELLA_TEST_SRC}.c")
    LIST(APPEND YELLA_TEST_SRC ${ARGN})
    ADD_EXECUTABLE(${YELLA_TEST_NAME} EXCLUDE_FROM_ALL ${YELLA_TEST_SRC})
    TARGET_LINK_LIBRARIES(${YELLA_TEST_NAME} "${YELLA_CMOCKA_LIB}" agent ${YELLA_ZEROMQ_LIB} ${YELLA_UUID_LIB})
    ADD_TEST(NAME ${YELLA_TEST_NAME} COMMAND ${YELLA_TEST_NAME})
    ADD_DEPENDENCIES(no-check ${YELLA_TEST_NAME})
    ADD_DEPENDENCIES(all-targets ${YELLA_TEST_NAME})
ENDMACRO()

ADD_EXECUTABLE(fake-router EXCLUDE_FROM_ALL fake_router.c)
TARGET_LINK_LIBRARIES(fake-router agent)

ADD_SUBDIRECTORY(common)
ADD_SUBDIRECTORY(agent)