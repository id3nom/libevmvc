cmake_minimum_required(VERSION 3.0.0)
project(bbact VERSION 0.1.0 LANGUAGES CXX C)


# define module path
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake_modules" ${CMAKE_MODULE_PATH})
message(STATUS "** CMAKE_MODULE_PATH: '${CMAKE_MODULE_PATH}'")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# boost dependencies
set(Boost_ADDITIONAL_VERSIONS "1.69")

set(EVMVC_BOOST_COMPONENTS
    filesystem
    #atomic
    #date_time
    #system
    #iostreams
    regex
    program_options
    #thread
    #chrono
    #locale
)

find_package(Boost "1.69" REQUIRED COMPONENTS ${EVMVC_BOOST_COMPONENTS} )
message(STATUS "** BOOST Include: ${Boost_INCLUDE_DIR}")
message(STATUS "** BOOST Libraries Dirs: ${Boost_LIBRARY_DIRS}")
message(STATUS "** BOOST Libraries: ${Boost_LIBRARIES}")

include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

# OpenSSL
find_package(OpenSSL REQUIRED)
if (OPENSSL_FOUND)
    include_directories(${OPENSSL_INCLUDE_DIR})
endif()

# FindZLib
find_package(ZLIB REQUIRED)
if(ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
endif()

# FindICU
find_package(ICU REQUIRED)
if(ICU_FOUND)
    include_directories(${ICU_INCLUDE_DIRS})
endif()

# libevent
if(NOT LIBEVENT_CMake_INCLUDED)
    include(FindLibEvent)
endif()
find_package(LibEvent REQUIRED)
include_directories(${LIBEVENT_INCLUDE_DIRS})

message(STATUS "** OpenSSL includes: '${OPENSSL_INCLUDE_DIR}'")
message(STATUS "** ZLIB includes: '${ZLIB_INCLUDE_DIRS}'")
message(STATUS "** ICU includes: '${ICU_INCLUDE_DIRS}'")
message(STATUS "** libevent includes: '${LIBEVENT_INCLUDE_DIRS}'")

message(STATUS "** OpenSSL libs: '${OPENSSL_LIBRARIES}'")
message(STATUS "** ZLIB libs: '${ZLIB_LIBRARIES}'")
message(STATUS "** ICU libs: '${ICU_LIBRARIES}'")
message(STATUS "** libevent libs: '${LIBEVENT_LIBRARIES}'")

file(GLOB BBACT_SOURCES src/*.cpp)

add_executable(bbact ${BBACT_SOURCES})
target_link_libraries(bbact
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${ICU_LIBRARIES}
    pthread
    ${LIBEVENT_LIBRARIES}
)
set_target_properties(bbact PROPERTIES LINKER_LANGUAGE CXX)
