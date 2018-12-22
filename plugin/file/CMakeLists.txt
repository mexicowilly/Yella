INCLUDE_DIRECTORIES("${YELLA_SQLITE3_INCLUDE_DIR}"
                    "${OPENSSL_INCLUDE_DIR}")

IF(YELLA_FREEBSD)
    SET(FILE_PLATFORM_SOURCES
        platform/freebsd/event_source_freebsd.c)
ENDIF()

ADD_LIBRARY(file SHARED
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
            state_db.c
            state_db.h
            state_db_pool.c
            state_db_pool.h
            ${FILE_PLATFORM_SOURCES})
TARGET_LINK_LIBRARIES(file ${YELLA_SQLITE3_LIB} OpenSSL::Crypto plugin)
IF(YELLA_FREEBSD)
    TARGET_LINK_LIBRARIES(file procstat)
ENDIF()
SET_TARGET_PROPERTIES(file PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
ADD_DEPENDENCIES(all-targets file)

YELLA_GEN_TARGET(file
                 serialization/public/file.fbs)
ADD_DEPENDENCIES(file-gen plugin-gen)
