#
# Copyright (c) 2019 Carnegie Mellon University,
# Copyright (c) 2019 Triad National Security, LLC, as operator of
#     Los Alamos National Laboratory.
#
# All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file. See the AUTHORS file for names of contributors.
#

#
# CMakeLists.txt  cmake file for deltafs-shuffle src
#

# list of source files
set (deltafs-shuffle-srcs acnt_wrap.c shuf_mlog.cc  shuffle.cc)

#
# configure/load in standard modules we plan to use and probe the enviroment
#
include (CMakePackageConfigHelpers)

set (CMAKE_THREAD_PREFER_PTHREAD TRUE)
set (THREADS_PREFER_PTHREAD_FLAG TRUE)

find_package (Threads REQUIRED)

# where to install files for "find_package"
set (deltafs-shuffle-pkg-loc "share/cmake/deltafs-shuffle")

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
# BUILD_SHARED_LIBS).  arrange for users of our lib to get the correct "-I"'s
#
add_library (deltafs-shuffle ${deltafs-shuffle-srcs})
target_include_directories (deltafs-shuffle PUBLIC $<INSTALL_INTERFACE:include>)

# make sure our build includes are BEFORE a previously installed version
target_include_directories (deltafs-shuffle BEFORE PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>)

target_link_libraries (deltafs-shuffle deltafs-nexus)
target_link_libraries (deltafs-shuffle mercury)
target_link_libraries (deltafs-shuffle Threads::Threads)

set_target_properties(deltafs-shuffle
        PROPERTIES VERSION ${DELTAFS_SHUFFLE_VERSION}
        SOVERSION ${DELTAFS_SHUFFLE_VERSION_MAJOR})

#
# installation stuff (packaging and install commands)
#
write_basic_package_version_file ("deltafs-shuffle-config-version.cmake"
        VERSION ${DELTAFS_SHUFFLE_VERSION}
        COMPATIBILITY AnyNewerVersion)

# generate our config file for "find_package"
configure_file (deltafs-shuffle-config.cmake.in
                deltafs-shuffle-config.cmake @ONLY)

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
install (FILES ../include/deltafs-shuffle/shufapi.h
         DESTINATION include/deltafs-shuffle)
