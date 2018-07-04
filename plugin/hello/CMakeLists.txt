ADD_LIBRARY(hello SHARED
            hello.c
            ../plugin.h)
TARGET_LINK_LIBRARIES(hello plugin)

YELLA_GEN_TARGET(hello
                 serialization/public/hello.fbs)
ADD_DEPENDENCIES(hello-gen plugin-gen)

