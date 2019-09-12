SET(YELLA_GEN_DIR "${CMAKE_BINARY_DIR}/gen/console")
FILE(MAKE_DIRECTORY "${YELLA_GEN_DIR}")

INCLUDE_DIRECTORIES("${YELLA_FLATBUFFERS_INCLUDE_DIR}"
                    "${YELLA_CHUCHO_STATIC_INCLUDE_DIR}"
                    "${YELLA_YAML_CPP_INCLUDE_DIR}"
                    "${YELLA_RABBITMQ_INCLUDE_DIR}"
                    "${YELLA_POSTGRES_INCLUDE_DIR}"
                    "${QT_INCLUDE_DIR}"
                    "${YELLA_GEN_DIR}")

FOREACH(FBS
        "${CMAKE_SOURCE_DIR}/agent/common/serialization/public/parcel.fbs"
        "${CMAKE_SOURCE_DIR}/agent/agent/serialization/public/heartbeat.fbs")
    GET_FILENAME_COMPONENT(YELLA_GEN_BASE "${FBS}" NAME_WE)
    SET(YELLA_CUR_GEN "${YELLA_GEN_DIR}/${YELLA_GEN_BASE}_generated.h")
    ADD_CUSTOM_COMMAND(OUTPUT ${YELLA_CUR_GEN}
                       COMMAND "${YELLA_FLATC}" --cpp -o "${YELLA_GEN_DIR}" "${FBS}"
                       DEPENDS "${FBS}")
    LIST(APPEND YELLA_CONSOLE_SOURCES ${YELLA_CUR_GEN})
    SET_SOURCE_FILES_PROPERTIES(${YELLA_CUR_GEN} PROPERTIES GENERATED TRUE)
    ADD_CUSTOM_TARGET(console-${YELLA_GEN_BASE}-gen
                      DEPENDS "${YELLA_GEN_DIR}/${YELLA_GEN_BASE}_generated.h")
    LIST(APPEND YELLA_GEN_DEPS console-${YELLA_GEN_BASE}-gen)
ENDFOREACH()
ADD_CUSTOM_TARGET(console-gen DEPENDS ${YELLA_GEN_DEPS})

SET(YELLA_UI_GEN "${YELLA_GEN_DIR}/ui_main_window.hpp")
ADD_CUSTOM_COMMAND(OUTPUT ${YELLA_UI_GEN}
                   COMMAND Qt4::uic ARGS -o ${YELLA_UI_GEN} "${CMAKE_CURRENT_SOURCE_DIR}/main_window.ui"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/main_window.ui")
LIST(APPEND YELLA_CONSOLE_SOURCES ${YELLA_UI_GEN})
SET_SOURCE_FILES_PROPERTIES(${YELLA_UI_GEN} PROPERTIES GENERATED TRUE)
ADD_CUSTOM_TARGET(console-ui-gen DEPENDS ${YELLA_UI_GEN})
ADD_DEPENDENCIES(console-gen console-ui-gen)

LIST(APPEND YELLA_CONSOLE_SOURCES
     agent.cpp
     agent.hpp
     configuration.cpp
     configuration.hpp
     console.cpp
     console.hpp
     database.cpp
     database.hpp
     fatal_error.hpp
     main.cpp
     main_window.hpp
     main_window.cpp
     message_queue.cpp
     message_queue.hpp
     model.cpp
     model.hpp
     parcel.cpp
     parcel.hpp
     postgres_db.cpp
     postgres_db.hpp
     rabbitmq.cpp
     rabbitmq.hpp)

ADD_EXECUTABLE(yella-console ${YELLA_CONSOLE_SOURCES})
TARGET_LINK_LIBRARIES(yella-console
                      "${YELLA_CHUCHO_STATIC_LIB}"
                      "${YELLA_FLATBUFFERS_LIB}"
                      "${YELLA_YAML_CPP_LIB}"
                      "${YELLA_RABBITMQ_LIB}"
                      "${YELLA_POSTGRESXX_LIB}"
                      "${YELLA_POSTGRES_LIB}"
                      Qt4::QtGui
                      Threads::Threads)
ADD_DEPENDENCIES(yella-console console-gen)
ADD_DEPENDENCIES(all-targets yella-console)
