#
# CMakeLists.txt  cmake file for deltafs-shuffle src
#

# list of source files
set (deltafs-shuffle-srcs shuffle_internal.cc shuffle.cc
        shuffler/shuffler.cc
        shuffler/shuf_mlog.cc
        shuffler/acnt_wrap.c
        shuffler/mlog.c)

#
# configure/load in standard modules we plan to use and probe the enviroment
#
include (CMakePackageConfigHelpers)

set (CMAKE_THREAD_PREFER_PTHREAD TRUE)
set (THREADS_PREFER_PTHREAD_FLAG TRUE)

#
# library version set here (e.g. for shared libs).
#
set (DELTAFS_SHUFFLE_VERSION_MAJOR 1)
set (DELTAFS_SHUFFLE_VERSION_MINOR 0)
set (DELTAFS_SHUFFLE_VERSION_PATCH 0)
set (deltafs-shuffle-vers
        "${DELTAFS_SHUFFLE_VERSION_MAJOR}.${DELTAFS_SHUFFLE_VERSION_MINOR}")
set (DELTAFS_SHUFFLE_VERSION
        "${deltafs-shuffle-vers}.${DELTAFS_SHUFFLE_VERSION_PATCH}")

#
# create library target (user can specify shared vs. static using
# BUILD_SHARED_LIBS).  arrange for users of our lib to get the correct
# -I's for our include directories.
#
add_library (deltafs-shuffle ${deltafs-shuffle-srcs})

target_include_directories (deltafs-shuffle PUBLIC $<INSTALL_INTERFACE:include>)
# make sure our build includes are BEFORE a previously installed version
target_include_directories (deltafs-shuffle BEFORE PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)

target_link_libraries (deltafs-shuffle deltafs-nexus)
target_link_libraries (deltafs-shuffle mercury)
# XXX: cmake 3.1 and newer define a Threads::Threads imported target
# that we should switch too when we are ready to require 3.1 or better.
# (3.1 was released late 2014)
if (THREADS_HAVE_PTHREAD_ARG)
    target_compile_options(deltafs-shuffle PUBLIC "-pthread")
endif ()
if (CMAKE_THREAD_LIBS_INIT)
    target_link_libraries (deltafs-shuffle "${CMAKE_THREAD_LIBS_INIT}")
endif ()

set_target_properties(deltafs-shuffle
        PROPERTIES VERSION ${DELTAFS_SHUFFLE_VERSION}
        SOVERSION ${DELTAFS_SHUFFLE_VERSION_MAJOR})

# special handling for MPI, where the config comes in via MPI_<lang>_ vars.
foreach (lcv ${MPI_CXX_COMPILE_FLAGS_LIST})
    if (NOT ${lcv} STREQUAL "")
        target_compile_options (deltafs-shuffle PUBLIC $<BUILD_INTERFACE:${lcv}>)
    endif ()
endforeach ()
foreach (lcv ${MPI_CXX_INCLUDE_PATH})
    target_include_directories (deltafs-shuffle PUBLIC $<BUILD_INTERFACE:${lcv}>)
endforeach ()
foreach (lcv ${MPI_CXX_LIBRARIES})
    target_link_libraries(deltafs-shuffle $<BUILD_INTERFACE:${lcv}>)
endforeach ()

set_property (TARGET deltafs-shuffle APPEND PROPERTY LINK_FLAGS
        ${MPI_CXX_LINK_FLAGS})
get_property (link_flags TARGET deltafs-shuffle PROPERTY LINK_FLAGS)

#
# installation stuff (packaging and install commands)
#
write_basic_package_version_file(
        "deltafs-shuffle-config-version.cmake"
        VERSION ${DELTAFS_SHUFFLE_VERSION}
        COMPATIBILITY AnyNewerVersion)

# generate our config file for "find_package"
configure_file (deltafs-shuffle-config.cmake.in deltafs-shuffle-config.cmake @ONLY)

# where to install files for "find_package"
set (deltafs-shuffle-pkg-loc "share/cmake/deltafs-shuffle")

#
# "make install" rules
#
install (TARGETS deltafs-shuffle EXPORT deltafs-shuffle-targets
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
install (EXPORT deltafs-shuffle-targets
        DESTINATION ${deltafs-shuffle-pkg-loc}
        FILE "deltafs-shuffle-targets.cmake")
install (FILES "${CMAKE_CURRENT_BINARY_DIR}/deltafs-shuffle-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/deltafs-shuffle-config-version.cmake"
        DESTINATION ${deltafs-shuffle-pkg-loc} )
install (DIRECTORY ../include/deltafs-shuffle
        DESTINATION include
        FILES_MATCHING PATTERN "*.h")
