# Overrides Photon's bundled Finduring.cmake (this directory precedes
# Photon's CMake/ dir in CMAKE_MODULE_PATH). The bundled module demands that
# the literal file liburing.so.2.3 exist, which fails on any distro shipping
# a newer liburing (e.g. 2.14) even though liburing's ABI is stable within
# the liburing.so.2 soname. Enforce the ">= 2.3" intent instead by requiring
# liburing/io_uring_version.h, which first shipped in liburing 2.3.

find_path(URING_INCLUDE_DIRS liburing.h)

find_library(URING_LIBRARIES uring)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(uring DEFAULT_MSG URING_LIBRARIES URING_INCLUDE_DIRS)

mark_as_advanced(URING_INCLUDE_DIRS URING_LIBRARIES)

if (NOT EXISTS "${URING_INCLUDE_DIRS}/liburing/io_uring_version.h")
    message(FATAL_ERROR "Requires liburing >= 2.3. Install it to system or try -D PHOTON_BUILD_DEPENDENCIES=ON")
endif ()
