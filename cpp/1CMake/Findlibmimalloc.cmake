# Find libmimalloc
# creates variables LIBMIMALLOC_FOUND, LIBMIMALLOC_LIBRARY, LIBMIMALLOC_LIBRARY_DIR

find_library(LIBMIMALLOC_LIBRARY NAMES
  mimalloc
  libmimalloc
  libmimalloc.so.2
)
mark_as_advanced(LIBMIMALLOC_LIBRARY)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
  libmimalloc
  REQUIRED_VARS LIBMIMALLOC_LIBRARY)

get_filename_component(LIBMIMALLOC_LIBRARY_DIR ${LIBMIMALLOC_LIBRARY} DIRECTORY)
