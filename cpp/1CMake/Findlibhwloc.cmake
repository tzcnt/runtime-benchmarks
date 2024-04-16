# Find libhwloc
# creates variables LIBHWLOC_FOUND, LIBHWLOC_LIBRARY, LIBHWLOC_LIBRARY_DIR, LIBHWLOC_INCLUDE_DIR

# requires system package "libhwloc-dev" on Debian/Ubuntu
find_library(LIBHWLOC_LIBRARY NAMES hwloc)
mark_as_advanced(LIBHWLOC_LIBRARY)
find_path(LIBHWLOC_INCLUDE_DIR NAMES hwloc.h)
mark_as_advanced(LIBHWLOC_INCLUDE_DIR)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
  libhwloc
  REQUIRED_VARS LIBHWLOC_LIBRARY LIBHWLOC_INCLUDE_DIR)

get_filename_component(LIBHWLOC_LIBRARY_DIR ${LIBHWLOC_LIBRARY} DIRECTORY)
