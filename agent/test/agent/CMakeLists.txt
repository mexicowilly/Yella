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

CONFIGURE_FILE(test-agent.yaml.in ${CMAKE_BINARY_DIR}/test/agent/test-agent.yaml)

YELLA_TEST(spool-test)
YELLA_TEST(mac-addresses-test)
YELLA_TEST(saved-state-test)
YELLA_TEST(heartbeat-test)
YELLA_TEST(router-test)
