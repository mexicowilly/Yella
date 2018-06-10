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
    INCLUDE_DIRECTORIES("${YELLA_UUID_INCLUDE_DIR}")
ENDIF()

IF(YELLA_POSIX)
    SET(YELLA_PLATFORM_PART posix)
ENDIF()
IF(YELLA_MACINTOSH)
    SET(YELLA_MAC_ADDR_PLATFORM macintosh)
ENDIF()

YELLA_TEST(router-test
           ${CMAKE_SOURCE_DIR}/agent/platform/${YELLA_PLATFORM_PART}/yella_uuid_${YELLA_PLATFORM_PART}.c
           ${CMAKE_SOURCE_DIR}/agent/router.c)
YELLA_TEST(spool-test ${CMAKE_SOURCE_DIR}/agent/spool.c)
YELLA_TEST(mac-addresses-test
           ${CMAKE_SOURCE_DIR}/agent/mac_addresses.c
           ${CMAKE_SOURCE_DIR}/agent/platform/${YELLA_MAC_ADDR_PLATFORM}/mac_addresses_${YELLA_MAC_ADDR_PLATFORM}.c)
IF(YELLA_MACINTOSH)
    TARGET_LINK_LIBRARIES(mac-addresses-test ${YELLA_IO_KIT} ${YELLA_CORE_FOUNDATION})
ENDIF()
YELLA_TEST(saved-state-test
           ${CMAKE_SOURCE_DIR}/agent/saved_state.c
           ${CMAKE_SOURCE_DIR}/agent/platform/${YELLA_PLATFORM_PART}/yella_uuid_${YELLA_PLATFORM_PART}.c
           ${CMAKE_SOURCE_DIR}/agent/mac_addresses.c
           ${CMAKE_SOURCE_DIR}/agent/platform/${YELLA_MAC_ADDR_PLATFORM}/mac_addresses_${YELLA_MAC_ADDR_PLATFORM}.c)
TARGET_LINK_LIBRARIES(saved-state-test ${YELLA_FLATCC_LIB})
IF(YELLA_MACINTOSH)
    TARGET_LINK_LIBRARIES(saved-state-test ${YELLA_IO_KIT} ${YELLA_CORE_FOUNDATION})
ENDIF()
ADD_DEPENDENCIES(saved-state-test yella-gen)