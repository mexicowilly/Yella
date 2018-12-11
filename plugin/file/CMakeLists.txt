INCLUDE_DIRECTORIES("${YELLA_SQLITE3_INCLUDE_DIR}"
                    "${OPENSSL_INCLUDE_DIR}")
ADD_LIBRARY(file SHARED
            attribute.c
            attribute.h
            collect_attributes.c
            collect_attributes.h
            element.c
            element.h
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
            state_db_pool.h)
TARGET_LINK_LIBRARIES(file ${YELLA_SQLITE3_LIB} OpenSSL::Crypto plugin)
SET_TARGET_PROPERTIES(file PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
ADD_DEPENDENCIES(all-targets file)

YELLA_GEN_TARGET(file
                 serialization/public/file.fbs)
ADD_DEPENDENCIES(file-gen plugin-gen)
