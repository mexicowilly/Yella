SET(YELLA_GEN_DIR "${CMAKE_BINARY_DIR}/gen/router")
FILE(MAKE_DIRECTORY "${YELLA_GEN_DIR}")

INCLUDE_DIRECTORIES("${YELLA_FLATBUFFERS_INCLUDE_DIR}"
                    "${YELLA_CHUCHO_STATIC_INCLUDE_DIR}"
                    "${YELLA_YAML_CPP_INCLUDE_DIR}"
                    "${YELLA_ZEROMQ_INCLUDE_DIR}"
                    "${YELLA_RABBITMQ_INCLUDE_DIR}"
                    "${YELLA_GEN_DIR}"
                    "${CMAKE_SOURCE_DIR}/agent")

ADD_CUSTOM_COMMAND(OUTPUT "${YELLA_GEN_DIR}/parcel_generated.h"
                   COMMAND "${YELLA_FLATC}" --cpp -o "${YELLA_GEN_DIR}" "${CMAKE_SOURCE_DIR}/agent/common/serialization/public/parcel.fbs")
ADD_CUSTOM_TARGET(flatb-gen
                  DEPENDS "${YELLA_GEN_DIR}/parcel_generated.h")

SET(YELLA_ROUTER_SOURCES
    agent_face.cpp
    agent_face.hpp
    bucket_brigade.cpp
    bucket_brigade.hpp
    configuration.cpp
    configuration.hpp
    face.hpp
    fatal_error.hpp
    main.cpp
    mq_face.cpp
    mq_face.hpp
    rabbit_mq_face.cpp
    rabbit_mq_face.hpp
    ${CMAKE_SOURCE_DIR}/agent/agent/signal_handler.h
    zeromq_agent_face.cpp
    zeromq_agent_face.hpp)

IF(YELLA_POSIX)
    LIST(APPEND YELLA_ROUTER_SOURCES
         ${CMAKE_SOURCE_DIR}/agent/agent/platform/posix/signal_handler_posix.c)
ENDIF()

ADD_EXECUTABLE(yella-router ${YELLA_ROUTER_SOURCES})
TARGET_LINK_LIBRARIES(yella-router
                      "${YELLA_CHUCHO_STATIC_LIB}"
                      "${YELLA_FLATBUFFERS_LIB}"
                      "${YELLA_YAML_CPP_LIB}"
                      "${YELLA_ZEROMQ_LIB}"
                      "${YELLA_RABBITMQ_LIB}"
                      Threads::Threads)
ADD_DEPENDENCIES(yella-router flatb-gen)
ADD_DEPENDENCIES(all-targets yella-router)
