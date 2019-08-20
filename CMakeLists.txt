cmake_minimum_required(VERSION 3.0.0)
cmake_policy(SET CMP0054 NEW)
project(libevmvc VERSION 0.1.0 LANGUAGES CXX C)

# define module path
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake" ${CMAKE_MODULE_PATH})

if(NOT PrecompiledHeader_CMake_INCLUDED)
    include(PrecompiledHeader)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

option(EVMVC_BUILD_TESTS "Build tests" OFF)
option(EVMVC_BUILD_FANJET "Build fanjet precompiler tool" ON)
option(EVMVC_BUILD_EXAMPLES "Build examples" OFF)
option(EVMVC_BUILD_DOC "Create and install the HTML based API documentation (requires Doxygen)" OFF)

# not using unicode at all, better using UTF-8
# add_definitions(-DUNICODE -D_UNICODE)

if(UNIX)
    message(STATUS " Setting GCC flags")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -g -Wall -Werror -fmax-errors=10 -msse4.1")
    
else(UNIX)
    message(STATUS " Setting MSVC flags")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3 /arch:SSE2")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /VERBOSE:LIB ")
    
endif(UNIX)
message(STATUS "** CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")

# boost dependencies
set(Boost_ADDITIONAL_VERSIONS "1.69")

#set(Boost_ROOT "" CACHE PATH "Boost root dir")
set(Boost_DEBUG  ON CACHE BOOL "Boost_DEBUG")
set(Boost_USE_STATIC_LIBS        ON CACHE BOOL "Boost_USE_STATIC_LIBS")
set(Boost_USE_MULTITHREADED      ON CACHE BOOL "Boost_USE_MULTITHREADED")
set(Boost_USE_STATIC_RUNTIME     OFF CACHE BOOL "Boost_USE_STATIC_RUNTIME")
set(Boost_ALL_DYN_LINK           OFF CACHE BOOL "BOOST_ALL_DYN_LINK") 

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

if(NOT Boost_LIBRARIES)
    set(Boost_LIBRARIES Boost::filesystem;Boost::regex;Boost::program_options)
endif()


include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

# OpenSSL
find_package(OpenSSL REQUIRED)
if (OPENSSL_FOUND)
    include_directories (${OPENSSL_INCLUDE_DIR})
endif()

# FindZLib
find_package(ZLIB REQUIRED)
if (ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
endif()

# FindICU
if(NOT TARGET "ICU::UC")
    find_package(ICU REQUIRED)
    if (ICU_FOUND)
        include_directories(${ICU_INCLUDE_DIRS})
    endif()
endif()

# # find spdlog
# find_package(spdlog CONFIG REQUIRED)


# # libevent
# if(NOT LIBEVENT_CMake_INCLUDED)
#     include(FindLibEvent)
# endif()
# find_package(LibEvent REQUIRED)
# include_directories (${LIBEVENT_INCLUDE_DIR})

# static or shared build configuration
option(BUILD_SHARED_LIBS "Build shared libraries (DLLs)." OFF)
if(${BUILD_SHARED_LIBS})
    set(IS_MONOLITHIC 0)
else()
    set(IS_MONOLITHIC 1)
endif()


# debug vs test vs shipping build type
set(EVMVC_BUILD_TYPE "DEBUG" CACHE STRING "libevmvc BUILD TYPE")
set(EVMVC_BUILD_TYPE_VALUES "DEBUG;RELEASE;")
set_property(CACHE EVMVC_BUILD_TYPE PROPERTY STRINGS ${EVMVC_BUILD_TYPE_VALUES})

message(STATUS "libevmvc build type='${EVMVC_BUILD_TYPE}'")
if( ${EVMVC_BUILD_TYPE} STREQUAL "DEBUG")
    set(EVMVC_BUILD_DEBUG 1)
else(${EVMVC_BUILD_TYPE} STREQUAL "DEBUG")
    set(EVMVC_BUILD_RELEASE 1)
endif(${EVMVC_BUILD_TYPE} STREQUAL "DEBUG")

set(BITNESS 32)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(BITNESS 64)
endif()

#############
# add subdirs

# deps
if(NOT TARGET tz)
    subdirs(deps/date)
endif()
if(NOT TARGET fmt)
    subdirs(deps/fmt)
endif()
# if(NOT TARGET json) # not working with debian stretch
#     subdirs(deps/json)
# endif()

if(NOT TARGET gmock)
    subdirs(deps/googletest)
endif()

# lib dir

# tests
if(EVMVC_BUILD_TESTS)
    subdirs(tests-evmvc)
    add_test(alltests ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/libevmvc_tests)
    enable_testing()
endif()

# fanjet
if(EVMVC_BUILD_FANJET)
    subdirs(fanjet-precompiler)
endif()

# examples
if(EVMVC_BUILD_EXAMPLES)
    # fanjet tool is required with examples
    if(NOT EVMVC_BUILD_FANJET)
        subdirs(fanjet-precompiler)
    endif()
    subdirs(examples-evmvc)
endif()

# doc
if(EVMVC_BUILD_DOC)
    subdirs(doc-evmvc)
endif()

#######
# cpack
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
if(NOT CPack_CMake_INCLUDED)
    include(CPack)
endif()



################
### libevmvc ###
################

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/libevmvc_config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/evmvc/evmvc_config.h"
)

# build thread safe components
option(EVMVC_THREAD_SAFE "Enable thread safety." ON)

# add the binary tree to the search path for include files
include_directories("${PROJECT_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/evmvc"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/date/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/fmt/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/json/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/tools-md/include"
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
file(GLOB EVMVC_HEADERS "${PROJECT_SOURCE_DIR}/include/evmvc/*.h")

source_group("Headers" FILES ${EVMVC_HEADERS})

file(GLOB EVMVC_SOURCES src/*.cpp)

include(GenerateExportHeader)

if(${BUILD_SHARED_LIBS})
    add_library(evmvc SHARED
        ${EVMVC_SOURCES}
        ${EVMVC_HEADERS}
    )
else()
    add_library(evmvc STATIC
        ${EVMVC_SOURCES}
        ${EVMVC_HEADERS}
    )
endif()
set_target_properties(evmvc PROPERTIES LINKER_LANGUAGE CXX)

generate_export_header(evmvc
    EXPORT_MACRO_NAME "EVMVC_CLIENT_API"
    EXPORT_FILE_NAME "${PROJECT_SOURCE_DIR}/include/evmvc/evmvc_export.h"
)

if(WIN32)
   set_target_properties(evmvc PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
   set_target_properties(evmvc PROPERTIES COMPILE_DEFINITIONS_DEBUG "_CONSOLE")
   set_target_properties(evmvc PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE")
   set_target_properties(evmvc PROPERTIES COMPILE_DEFINITIONS_RELWITHDEBINFO "_CONSOLE")
   #set_target_properties(evmvc PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
   #set_target_properties(evmvc PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS")
   set_target_properties(evmvc PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE")
   set_target_properties(evmvc PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:CONSOLE")
endif(WIN32)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    #add_precompiled_header(xpzs ServerPCH.h 
    #   FORCEINCLUDE
    #   #SOURCE_CXX:CorePrivatePCH.cpp
    #   SOURCE_CXX "${PROJECT_SOURCE_DIR}/server/src/ServerPCH.cpp"
    #)

    # add required windows compile flags
    SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Zm200 -wd4503 -bigobj " )
    
    # add required windows libs
    if(${EVMVC_BUILD_DEBUG})
        TARGET_LINK_LIBRARIES(evmvc Ws2_32.lib Setupapi.lib Dbghelp.lib Winmm.lib Iphlpapi.lib Netapi32.lib Imm32.lib Xinput.lib Dwmapi.lib Wininet.lib )
        
    else(${EVMVC_BUILD_DEBUG})
        TARGET_LINK_LIBRARIES(evmvc Ws2_32.lib Setupapi.lib Dbghelp.lib Winmm.lib Iphlpapi.lib Netapi32.lib Imm32.lib Dwmapi.lib Xinput.lib Wininet.lib )
        
    endif(${EVMVC_BUILD_DEBUG})

else(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    #SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -include ${PROJECT_SOURCE_DIR}/server/inc/ServerPCH.h" )
    
endif(CMAKE_SYSTEM_NAME STREQUAL "Windows")

target_link_libraries(evmvc 
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

# install
install( TARGETS evmvc
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
install( DIRECTORY include/ DESTINATION include/ )

