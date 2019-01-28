cmake_minimum_required(VERSION 3.0.0)
cmake_policy(SET CMP0054 NEW)
project(libevent-mvc VERSION 0.1.0 LANGUAGES CXX C)

# define module path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/CMake")

if(NOT PrecompiledHeader_CMake_INCLUDED)
    include(PrecompiledHeader)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
#set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
set(WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

option(EventMvc_BUILD_TESTS "Build tests" OFF)
option(EventMvc_BUILD_DOC "Create and install the HTML based API documentation (requires Doxygen)" OFF)

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

set(EventMvc_BOOST_COMPONENTS
    #filesystem
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

find_package(Boost "1.69" REQUIRED COMPONENTS ${EventMvc_BOOST_COMPONENTS} )
message(STATUS "** BOOST Include: ${Boost_INCLUDE_DIR}")
message(STATUS "** BOOST Libraries Dirs: ${Boost_LIBRARY_DIRS}")
message(STATUS "** BOOST Libraries: ${Boost_LIBRARIES}")

include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})

# OpenSSL

find_package(OpenSSL)

if (OPENSSL_FOUND)
    include_directories (${OPENSSL_INCLUDE_DIR})
endif()


# static or shared build configuration
option(BUILD_SHARED_LIBS "Build shared libraries (DLLs)." OFF)
if(${BUILD_SHARED_LIBS})
    set(IS_MONOLITHIC 0)
else()
    set(IS_MONOLITHIC 1)
endif()


# debug vs test vs shipping build type
set(EventMvc_BUILD_TYPE "DEBUG" CACHE STRING "libpq-async++ BUILD TYPE")
set(EventMvc_BUILD_TYPE_VALUES "DEBUG;TEST;SHIPPING;")
set_property(CACHE EventMvc_BUILD_TYPE PROPERTY STRINGS ${EventMvc_BUILD_TYPE_VALUES})

message(STATUS "libpq-async++ build type='${EventMvc_BUILD_TYPE}'")
if( ${EventMvc_BUILD_TYPE} STREQUAL "DEBUG")
    set(EventMvc_BUILD_DEBUG 1)
elseif( ${EventMvc_BUILD_TYPE} STREQUAL "TEST")
    set(EventMvc_BUILD_TEST 1)
else(${EventMvc_BUILD_TYPE} STREQUAL "DEBUG")
    set(EventMvc_BUILD_SHIPPING 1)
endif(${EventMvc_BUILD_TYPE} STREQUAL "DEBUG")

set(BITNESS 32)
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(BITNESS 64)
endif()

#############
# add subdirs

# deps
if(NOT TARGET date)
    subdirs(deps/date)
endif()
if(NOT TARGET fmt)
    subdirs(deps/fmt)
endif()
if(NOT TARGET json)
    subdirs(deps/json)
endif()

if(NOT TARGET gmock)
    subdirs(deps/googletest)
endif()

# lib dir

# tests
if(EventMvc_BUILD_TESTS)
    subdirs(tests-event-mvc)
    add_test(alltests ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/boost-mvc_tests)
    enable_testing()
endif()
# doc
if(EventMvc_BUILD_DOC)
    subdirs(doc-event-mvc)
endif()

#######
# cpack
set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
if(NOT CPack_CMake_INCLUDED)
    include(CPack)
endif()



####################
### libevent-mvc ###
####################

# configure a header file to pass some of the CMake settings
# to the source code
configure_file (
    "${CMAKE_CURRENT_SOURCE_DIR}/libevent_mvc_config.h.in"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/event-mvc/mvc_config.h"
)

# build thread safe components
option(EventMvc_THREAD_SAFE "Enable thread safety." ON)

# add the binary tree to the search path for include files
include_directories("${PROJECT_BINARY_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/event-mvc"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/date/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/fmt/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/deps/json/include"
)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
file(GLOB EventMvc_HEADERS "${PROJECT_SOURCE_DIR}/include/event-mvc/*.h")

source_group("Headers" FILES ${EventMvc_HEADERS})

file(GLOB EventMvc_SOURCES src/*.cpp)

include(GenerateExportHeader)

if(${BUILD_SHARED_LIBS})
    add_library(event-mvc SHARED
        ${EventMvc_SOURCES}
        ${EventMvc_HEADERS}
    )
else()
    add_library(event-mvc STATIC
        ${EventMvc_SOURCES}
        ${EventMvc_HEADERS}
    )
endif()
set_target_properties(event-mvc PROPERTIES LINKER_LANGUAGE CXX)

generate_export_header(event-mvc
    EXPORT_MACRO_NAME "EventMvc_CLIENT_API"
    EXPORT_FILE_NAME "${PROJECT_SOURCE_DIR}/include/event-mvc/event_mvc_export.h"
)

if(WIN32)
   set_target_properties(event-mvc PROPERTIES LINK_FLAGS_DEBUG "/SUBSYSTEM:CONSOLE")
   set_target_properties(event-mvc PROPERTIES COMPILE_DEFINITIONS_DEBUG "_CONSOLE")
   set_target_properties(event-mvc PROPERTIES LINK_FLAGS_RELWITHDEBINFO "/SUBSYSTEM:CONSOLE")
   set_target_properties(event-mvc PROPERTIES COMPILE_DEFINITIONS_RELWITHDEBINFO "_CONSOLE")
   #set_target_properties(event-mvc PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS")
   #set_target_properties(event-mvc PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:WINDOWS")
   set_target_properties(event-mvc PROPERTIES LINK_FLAGS_RELEASE "/SUBSYSTEM:CONSOLE")
   set_target_properties(event-mvc PROPERTIES LINK_FLAGS_MINSIZEREL "/SUBSYSTEM:CONSOLE")
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
    if(${EventMvc_BUILD_DEBUG})
        TARGET_LINK_LIBRARIES(event-mvc Ws2_32.lib Setupapi.lib Dbghelp.lib Winmm.lib Iphlpapi.lib Netapi32.lib Imm32.lib Xinput.lib Dwmapi.lib Wininet.lib )
        
    else(${EventMvc_BUILD_DEBUG})
        TARGET_LINK_LIBRARIES(event-mvc Ws2_32.lib Setupapi.lib Dbghelp.lib Winmm.lib Iphlpapi.lib Netapi32.lib Imm32.lib Dwmapi.lib Xinput.lib Wininet.lib )
        
    endif(${EventMvc_BUILD_DEBUG})

else(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    #SET( CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -include ${PROJECT_SOURCE_DIR}/server/inc/ServerPCH.h" )
    
endif(CMAKE_SYSTEM_NAME STREQUAL "Windows")

target_link_libraries(event-mvc 
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    pthread
    tz
    fmt
    pcre
)

# install
install( TARGETS event-mvc
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})  # This is for Windows
install( DIRECTORY include/ DESTINATION include/ )

