SET(YELLA_GEN_DIR "${CMAKE_BINARY_DIR}/gen/fake-agent")
FILE(MAKE_DIRECTORY "${YELLA_GEN_DIR}")

INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}"
                    "${YELLA_GEN_DIR}")

ADD_CUSTOM_COMMAND(OUTPUT "${YELLA_GEN_DIR}/parcel_generated.h"
                   COMMAND "${YELLA_FLATC}" --cpp -o "${YELLA_GEN_DIR}" "${CMAKE_SOURCE_DIR}/agent/common/serialization/public/parcel.fbs")
ADD_CUSTOM_COMMAND(OUTPUT "${YELLA_GEN_DIR}/plugin_generated.h"
                   COMMAND "${YELLA_FLATC}" --cpp -o "${YELLA_GEN_DIR}" "${CMAKE_SOURCE_DIR}/agent/plugin/serialization/public/plugin.fbs")
ADD_CUSTOM_COMMAND(OUTPUT "${YELLA_GEN_DIR}/file_generated.h"
                   COMMAND "${YELLA_FLATC}" --cpp -o "${YELLA_GEN_DIR}" -I "${CMAKE_SOURCE_DIR}/agent/plugin/serialization/public" "${CMAKE_SOURCE_DIR}/agent/plugin/file/serialization/public/file.fbs")
ADD_CUSTOM_TARGET(fake-agent-gen
                  DEPENDS
                  "${YELLA_GEN_DIR}/parcel_generated.h"
                  "${YELLA_GEN_DIR}/plugin_generated.h"
                  "${YELLA_GEN_DIR}/file_generated.h")

SET(FAKE_AGENT_SOURCES
    agent.cpp
    agent.hpp
    cxxopts.hpp
    expected_exception.cpp
    expected_exception.hpp
    file_test_impl.cpp
    file_test_impl.hpp
    main.cpp
    parcel.cpp
    parcel.hpp
    plugin.cpp
    plugin.hpp
    plugin_message_receiver.hpp
    test.cpp
    test.hpp
    test_impl.cpp
    test_impl.hpp)

IF(YELLA_POSIX)
    LIST(APPEND FAKE_AGENT_SOURCES
         platform/posix/agent_posix.cpp
         platform/posix/file_test_impl_posix.cpp)
ENDIF()

ADD_EXECUTABLE(fake-agent ${FAKE_AGENT_SOURCES})
TARGET_LINK_LIBRARIES(fake-agent
                      plugin
                      "${YELLA_CHUCHO_LIB}"
                      "${YELLA_FLATBUFFERS_LIB}"
                      "${YELLA_YAML_CPP_LIB}"
                      OpenSSL::Crypto
                      stdc++fs)
IF(YELLA_LINUX)
    TARGET_LINK_LIBRARIES(fake-agent dl)
ENDIF()

ADD_DEPENDENCIES(fake-agent fake-agent-gen)
ADD_DEPENDENCIES(fake-agent file)
ADD_DEPENDENCIES(all-targets fake-agent)
