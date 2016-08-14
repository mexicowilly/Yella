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

IF(YELLA_POSIX)
    ADD_DEFINITIONS(-DYELLA_POSIX)
ENDIF()

INCLUDE_DIRECTORIES("${YELLA_CMOCKA_INCLUDE_DIR}")

MACRO(YELLA_TEST YELLA_TEST_NAME)
    ADD_EXECUTABLE(${YELLA_TEST_NAME} ${YELLA_TEST_NAME}.c)
    TARGET_LINK_LIBRARIES(${YELLA_TEST_NAME} "${YELLA_CMOCKA_LIB}" common)
    ADD_TEST(NAME ${YELLA_TEST_NAME} COMMAND ${YELLA_TEST_NAME})
ENDMACRO()

YELLA_TEST(settings_test)
