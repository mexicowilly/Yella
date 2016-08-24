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

YELLA_TEST(router_test ${YELLA_ZEROMQ_LIB} ${YELLA_UUID_LIB})
IF(YELLA_POSIX)
    TARGET_INCLUDE_DIRECTORIES(router_test PRIVATE
                               "${YELLA_UUID_INCLUDE_DIR}")
ENDIF()

