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

ADD_EXECUTABLE(fake-router EXCLUDE_FROM_ALL
               fake_router.c
               fake_router.h
               main.c
               spool_test.c
               spool_test.h)

TARGET_LINK_LIBRARIES(fake-router
                      common
                      "${YELLA_ZEROMQ_LIB}")

YELLA_GEN_TARGET(fake-router
                 ../serialization/spool_test.fbs)