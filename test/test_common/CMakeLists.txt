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

INCLUDE_DIRECTORIES("${YELLA_FLATCC_INCLUDE_DIR}")

ADD_LIBRARY(test-common STATIC EXCLUDE_FROM_ALL
            spool_test.c
            spool_test.h)

TARGET_LINK_LIBRARIES(test-common common)

YELLA_GEN_TARGET(test-common
                 "${CMAKE_CURRENT_SOURCE_DIR}/../serialization/spool_test.fbs")
