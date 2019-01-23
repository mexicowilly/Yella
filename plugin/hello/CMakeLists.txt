ADD_LIBRARY(hello SHARED
            hello.c
            ../plugin.h)
IF(DEFINED YELLA_SO_FLAGS)
    SET_TARGET_PROPERTIES(hello PROPERTIES
                          COMPILE_FLAGS "${YELLA_SO_FLAGS}")
ENDIF()
TARGET_LINK_LIBRARIES(hello plugin)
SET_TARGET_PROPERTIES(hello PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)
ADD_DEPENDENCIES(all-targets hello)

YELLA_GEN_TARGET(hello
                 serialization/public/hello.fbs)
ADD_DEPENDENCIES(hello-gen plugin-gen)

