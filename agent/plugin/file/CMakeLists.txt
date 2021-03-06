SET(FILE_SO_VERSION 1)

INCLUDE_DIRECTORIES("${YELLA_SQLITE3_INCLUDE_DIR}"
                    "${OPENSSL_INCLUDE_DIR}"
                    "${YELLA_ACL_INCLUDE_DIR}")

IF(YELLA_POSIX)
    SET(FILE_PLATFORM_SOURCES
        platform/posix/posix_acl.c
        platform/posix/stat_collector_posix.c
        platform/posix/user_group_posix.c)
ENDIF()
IF(YELLA_FREEBSD)
    LIST(APPEND FILE_PLATFORM_SOURCES
        platform/freebsd/event_source_freebsd.c)
ELSEIF(YELLA_LIBFSWATCH)
    LIST(APPEND FILE_PLATFORM_SOURCES
        platform/fswatch/event_source_fswatch.c)
ENDIF()

ADD_LIBRARY(file SHARED
            accumulator.c
            accumulator.h
            attribute.c
            attribute.h
            collect_attributes.c
            collect_attributes.h
            element.c
            element.h
            event_source.c
            event_source.h
            file_name_matcher.c
            file_name_matcher.h
            file_plugin.c
            job.c
            job.h
            job_queue.c
            job_queue.h
            ../plugin.h
            posix_acl.h
            stat_collector.h
            state_db.c
            state_db.h
            state_db_pool.c
            state_db_pool.h
            user_group.h
            ${FILE_PLATFORM_SOURCES})
SET_TARGET_PROPERTIES(file PROPERTIES
#                      VERSION ${YELLA_VERSION}
#                      SOVERSION ${FILE_SO_VERSION}
                      COMPILE_DEFINITIONS yella_EXPORTS)
IF(DEFINED YELLA_SO_FLAGS)
    SET_TARGET_PROPERTIES(file PROPERTIES
                          COMPILE_FLAGS "${YELLA_SO_FLAGS}")
ENDIF()
TARGET_LINK_LIBRARIES(file ${YELLA_SQLITE3_LIB} OpenSSL::Crypto plugin)

IF(YELLA_FREEBSD)
    TARGET_LINK_LIBRARIES(file procstat)
ENDIF()
IF(YELLA_LIBFSWATCH)
    TARGET_LINK_LIBRARIES(file "${YELLA_LIBFSWATCH_LIB}")
ENDIF()
IF(YELLA_POSIX)
    TARGET_LINK_LIBRARIES(file "${YELLA_ACL_LIB}")
ENDIF()
SET_TARGET_PROPERTIES(file PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
ADD_DEPENDENCIES(all-targets file)

LIST(APPEND YELLA_FLATCC_INCLUDE_DIRS agent/plugin/file/serialization/public)

YELLA_GEN_TARGET(file
                 serialization/public/file.fbs
                 serialization/private/file_config.fbs
                 serialization/private/db_attrs.fbs)
ADD_DEPENDENCIES(file-gen plugin-gen)
