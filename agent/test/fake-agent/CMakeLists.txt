INCLUDE_DIRECTORIES("${CMAKE_CURRENT_SOURCE_DIR}"
                    "${CMAKE_SOURCE_DIR}/agent/plugin")

SET(FAKE_AGENT_SOURCES
    agent.cpp
    agent.hpp
    cxxopts.hpp
    file_test_impl.cpp
    file_test_impl.hpp
    main.cpp
    plugin.cpp
    plugin.hpp
    plugin_message_receiver.hpp
    test.cpp
    test.hpp
    test_impl.cpp
    test_impl.hpp)

IF(YELLA_POSIX)
    LIST(APPEND FAKE_AGENT_SOURCES
         platform/posix/agent_posix.cpp)
ENDIF()

ADD_EXECUTABLE(fake-agent ${FAKE_AGENT_SOURCES})
TARGET_LINK_LIBRARIES(fake-agent
                      plugin
                      "${YELLA_CHUCHO_LIB}"
                      "${YELLA_FLATBUFFERS_LIB}"
                      "${YELLA_YAML_CPP_LIB}"
                      stdc++fs)
IF(YELLA_LINUX)
    TARGET_LINK_LIBRARIES(fake-agent dl)
ENDIF()

ADD_DEPENDENCIES(fake-agent file)