# Find libtcmalloc
# creates variables LIBTCMALLOC_FOUND, LIBTCMALLOC_LIBRARY, LIBTCMALLOC_LIBRARY_DIR

find_library(LIBTCMALLOC_LIBRARY NAMES
  tcmalloc_minimal
  libtcmalloc_minimal
  libtcmalloc_minimal.so.4
  tcmalloc
  libtcmalloc
  libtcmalloc.so.4
)
mark_as_advanced(LIBTCMALLOC_LIBRARY)

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
  libtcmalloc
  REQUIRED_VARS LIBTCMALLOC_LIBRARY)

get_filename_component(LIBTCMALLOC_LIBRARY_DIR ${LIBTCMALLOC_LIBRARY} DIRECTORY)
