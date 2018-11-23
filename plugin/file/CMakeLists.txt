INCLUDE_DIRECTORIES("${YELLA_SQLITE3_INCLUDE_DIR}")
ADD_LIBRARY(file SHARED
            file_plugin.c
            job.c
            job.h
            ../plugin.h)
TARGET_LINK_LIBRARIES(file ${YELLA_SQSLITE3_LIB} plugin)
SET_TARGET_PROPERTIES(file PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
ADD_DEPENDENCIES(all-targets file)

YELLA_GEN_TARGET(file
                 serialization/public/file.fbs)
ADD_DEPENDENCIES(file-gen plugin-gen)
