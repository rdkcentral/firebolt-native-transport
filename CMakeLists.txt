# Copyright 2023 Comcast Cable Communications Management, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.10)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

project(FireboltTransport)

# <VERSION_BLOCK>
if (NOT PROJECT_VERSION)
    find_package(Git QUIET)
    if (GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --match "v[0-9]*"
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            OUTPUT_VARIABLE GIT_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif ()

    if (GIT_VERSION)
        string(REGEX REPLACE "^v" "" GIT_VERSION "${GIT_VERSION}")
    else()
        set(GIT_VERSION "0.1.0-unknown")
    endif ()
    set(PROJECT_VERSION "${GIT_VERSION}" CACHE STRING "Project version string")
endif ()
# </VERSION_BLOCK>

set(VERSION ${PROJECT_VERSION})
message(STATUS "Project version: ${PROJECT_VERSION}")

option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(ENABLE_UNIT_TESTS "Build openrpc native test" OFF)

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${SYSROOT_PATH}/usr" CACHE INTERNAL "" FORCE)
    set(CMAKE_PREFIX_PATH ${SYSROOT_PATH}/usr/lib/cmake CACHE INTERNAL "" FORCE)
endif()

list(APPEND CMAKE_PREFIX_PATH
        "${SYSROOT_PATH}/usr/share/cmake")

list(APPEND CMAKE_MODULE_PATH
        "${SYSROOT_PATH}/usr/share/cmake"
        "${SYSROOT_PATH}/usr/lib/cmake"
        "${SYSROOT_PATH}/tools/cmake")

add_compile_options(-Wall -Wextra -Wpedantic)

add_subdirectory(src)
