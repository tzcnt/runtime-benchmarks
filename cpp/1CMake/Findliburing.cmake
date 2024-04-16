# Find liburing
# creates variables LIBURING_FOUND, LIBURING_LIBRARY, LIBURING_LIBRARY_DIR, LIBURING_INCLUDE_DIR

# requires system package "liburing-dev" on Debian/Ubuntu
find_library(LIBURING_LIBRARY NAMES uring)
mark_as_advanced(LIBURING_LIBRARY)
find_path(LIBURING_INCLUDE_DIR NAMES liburing.h)
mark_as_advanced(LIBURING_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
    liburing
    REQUIRED_VARS LIBURING_LIBRARY LIBURING_INCLUDE_DIR)

get_filename_component(LIBURING_LIBRARY_DIR ${LIBURING_LIBRARY} DIRECTORY)
