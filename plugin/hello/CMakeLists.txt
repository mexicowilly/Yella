ADD_LIBRARY(hello SHARED
            hello.c
            ../plugin.h)
TARGET_LINK_LIBRARIES(hello plugin)
SET_TARGET_PROPERTIES(hello PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins)

YELLA_GEN_TARGET(hello
                 serialization/public/hello.fbs)
ADD_DEPENDENCIES(hello-gen plugin-gen)

