# Find libjemalloc
# creates variables LIBJEMALLOC_FOUND, LIBJEMALLOC_LIBRARY, LIBJEMALLOC_LIBRARY_DIR

find_library(LIBJEMALLOC_LIBRARY NAMES
  jemalloc
  libjemalloc
  libjemalloc.so.2
)
mark_as_advanced(LIBJEMALLOC_LIBRARY)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
  libjemalloc
  REQUIRED_VARS LIBJEMALLOC_LIBRARY)

get_filename_component(LIBJEMALLOC_LIBRARY_DIR ${LIBJEMALLOC_LIBRARY} DIRECTORY)
