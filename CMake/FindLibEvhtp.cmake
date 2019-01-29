# - Try to find the LibEvHTP config processing library
# Once done this will define
#
# LIBEVHTP_FOUND - System has LibEvHTP
# LIBEVHTP_INCLUDE_DIR - the LibEvHTP include directory
# LIBEVHTP_LIBRARIES 0 The libraries needed to use LibEvHTP

if(LIBEVHTP_CMake_INCLUDED)
    message(WARNING "FindLibEvHTP.cmake has already been included!")
else()
    set(LIBEVHTP_CMake_INCLUDED 1)
endif()

find_path     (LIBEVHTP_INCLUDE_DIR NAMES evhtp.h)
find_library  (LIBEVHTP_LIBRARY     NAMES evhtp)

include (FindPackageHandleStandardArgs)

set (LIBEVHTP_INCLUDE_DIRS ${LIBEVHTP_INCLUDE_DIR})
set (LIBEVHTP_LIBRARIES
    ${LIBEVHTP_LIBRARY}
)

    find_package_handle_standard_args (LIBEVHTP DEFAULT_MSG LIBEVHTP_LIBRARIES LIBEVHTP_INCLUDE_DIR)
mark_as_advanced(LIBEVHTP_INCLUDE_DIRS LIBEVHTP_LIBRARIES)
