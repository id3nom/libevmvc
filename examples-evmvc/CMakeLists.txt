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

set (EVMVC_EXAMPLES_LIBRARIES
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


######################
## views precompile ##
######################

file(GLOB_RECURSE EVMVC_VIEWS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/views/" "*.fan"
)

message(STATUS "** EVMVC_VIEWS_SOURCES: ${EVMVC_VIEWS_SOURCES}")

set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/web-server.cpp"
    PROPERTIES
    OBJECT_DEPENDS "${EVMVC_VIEWS_SOURCES}"
)

set(jetfan-cmd "${CMAKE_BINARY_DIR}/bin/fanjet")
set(views-src-dir "${CMAKE_CURRENT_SOURCE_DIR}/views/")
set(views-dest-dir "${CMAKE_BINARY_DIR}/bin/examples-views/")

add_custom_command(OUTPUT "${views-dest-dir}examples_views.h"
    DEPENDS ${ALL_TAR_RES}
    COMMAND "${jetfan-cmd}" -v -n "examples" -i "examples_views.h" -s "${views-src-dir}" -d "${views-dest-dir}"
    COMMENT "Precompiling fanjet views from '${views-src-dir}' to '${views-dest-dir}'"
    VERBATIM
)

add_custom_target(
    examples_views ALL
    DEPENDS "${views-dest-dir}examples_views.h"
)

include_directories(
    "${views-dest-dir}"
)

######################
## views precompile ##
######################

add_executable(evmvc_web_server web-server.cpp)
add_dependencies(evmvc_web_server fanjet examples_views)
target_link_libraries(evmvc_web_server ${EVMVC_EXAMPLES_LIBRARIES})

