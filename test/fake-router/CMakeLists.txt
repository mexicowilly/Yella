CONFIGURE_FILE(router.yaml.in ${CMAKE_BINARY_DIR}/test/fake-router/router.yaml)

ADD_EXECUTABLE(fake-router EXCLUDE_FROM_ALL fake_router.c)
TARGET_LINK_LIBRARIES(fake-router agent)
ADD_DEPENDENCIES(all-targets fake-router)

