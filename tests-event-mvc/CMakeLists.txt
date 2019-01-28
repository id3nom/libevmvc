configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/../libevent_mvc_config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/event-mvc/mvc_config.h"
)

include_directories("${PROJECT_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../deps/date/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../deps/fmt/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../deps/json/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../deps/googletest/googletest/include/"
    "${CMAKE_CURRENT_SOURCE_DIR}/../deps/googletest/googlemock/include/"
)

set(test_sources
    main.cpp
    routing/router_tests.cpp
)
add_executable(libevent-mvc_tests ${test_sources})

target_link_libraries(libevent-mvc_tests
    event-mvc
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    pthread
    gmock
    pcre
    fmt
#    gtest
)
