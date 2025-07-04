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

cmake_minimum_required(VERSION 3.3)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(HelperFunctions)

project(FireboltTransport)
project_version("1.0.0")

set(FIREBOLT_TRANSPORT_NAMESPACE ${PROJECT_NAME} CACHE STRING "Namespace of the project")
set(FIREBOLT_TRANSPORT_WAITTIME 1000 CACHE STRING "Maximum time to wait for Transport layer to get response")
option(FIREBOLT_ENABLE_STATIC_LIB "Create Firebolt library as Static library" OFF)
option(ENABLE_UNIT_TESTS "Build openrpc native test" OFF)

if (FIREBOLT_ENABLE_STATIC_LIB)
    set(FIREBOLT_LIBRARY_TYPE STATIC)
    add_compile_definitions(FIREBOLTSDK_STATIC)
else ()
    set(FIREBOLT_LIBRARY_TYPE SHARED)
endif ()

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

find_package(WPEFramework CONFIG REQUIRED)

if (ENABLE_UNIT_TESTS)
    add_compile_definitions(UNIT_TEST UT_OPEN_RPC_FILE="firebolt-open-rpc.json")
endif ()

add_subdirectory(src)

message("${CMAKE_BINARY_DIR}/${PROJECT_NAME}Config.cmake")

configure_file("${CMAKE_SOURCE_DIR}/cmake/project.cmake.in"
    "${CMAKE_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    @ONLY)

