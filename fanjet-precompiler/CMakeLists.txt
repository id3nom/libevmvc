configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/../libevmvc_config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/evmvc/evmvc_config.h"
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

set (EVMVC_FANJET_LIBRARIES
    evmvc
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${ICU_LIBRARIES}
    pthread
    tz
    fmt
    pcre
    #${LIBEVENT_LIBRARIES}
    event
    event_core
    event_extra
    event_pthreads
    event_openssl
)

add_executable(fanjet main.cpp)
target_link_libraries(fanjet ${EVMVC_FANJET_LIBRARIES})

