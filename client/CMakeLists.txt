# Copyright (c) 2019 - present Advanced Micro Devices, Inc. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

message("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&")
message("                       Cmake Client Lib                            ")
message("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&")

## Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -m64")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse -msse2")
# Use this instead of above for 32 bit
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m32")

if("${CMAKE_BUILD_TYPE}" STREQUAL Release)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O2")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb -O0 -DDEBUG")
endif()


# Required Defines first:

message("")
message("Build Configuration:")
message("-------------BuildType: " ${CMAKE_BUILD_TYPE})
message("--------------Compiler: " ${CMAKE_CXX_COMPILER})
message("---------------Version: " ${CMAKE_CXX_COMPILER_VERSION})
message("----------Proj Src Dir: " ${PROJECT_SOURCE_DIR})
message("----------Proj Bld Dir: " ${PROJECT_BINARY_DIR})
message("----------Proj Lib Dir: " ${PROJECT_BINARY_DIR}/lib)
message("----------Proj Exe Dir: " ${PROJECT_BINARY_DIR}/bin)
message("----------RSMI Lib Dir: " ${RSMI_LIB_DIR})
message("----------RSMI Inc Dir: " ${RSMI_INC_DIR})
message("---------GRPC Root Dir: " ${GRPC_ROOT})
message("")

## Include common cmake modules
include(utils)

set(CLIENT_LIB "rdc_client_smi")
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/rdc")

################# Determine the library version #########################
## Setup the SO version based on git tags.
set(SO_VERSION_GIT_TAG_PREFIX "rdc_so_ver")

# provide git to utilities
find_program(GIT NAMES git)

# Debian package specific variables
# Set a default value for the package version
get_version_from_tag("1.0.0.0" ${SO_VERSION_GIT_TAG_PREFIX} GIT)

# VERSION_* variables should be set by get_version_from_tag
set(SO_VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}")
message("SOVERSION: ${SO_VERSION_STRING}")

set(CPACK_PACKAGE_FILE_NAME "${RDC_PACKAGE}-${VERSION_STRING}")

# TODO delete these if not used
file(GLOB PROTOBUF_GENERATED_INCLUDES "${PROTOB_OUT_DIR}/*.h")
file(GLOB PROTOBUF_GENERATED_SRCS "${PROTOB_OUT_DIR}/*.cc")

set(CLIENT_LIB_SRC_LIST "${SRC_DIR}/rdc_client.cc"
    "${SRC_DIR}/rdc_client_main.cc"
    "${SRC_DIR}/rdc_client_utils.cc"
    "${PROTOBUF_GENERATED_SRCS}"
    "${COMMON_DIR}/rdc_utils.cc")
message("CLIENT_LIB_SRC_LIST=${CLIENT_LIB_SRC_LIST}")

set(CLIENT_LIB_INC_LIST "${INC_DIR}/rdc_client.h"
    "${INC_DIR}/rdc_exception.h"
    "${INC_DIR}/rdc_client_main.h"
    "${COMMON_DIR}/rdc_utils.h")

add_library(${CLIENT_LIB} SHARED ${CLIENT_LIB_SRC_LIST} ${CLIENT_LIB_INC_LIST})
target_link_libraries(${CLIENT_LIB} pthread rt gRPC::grpc++ dl)
target_include_directories(${CLIENT_LIB} PRIVATE
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${PROTOB_OUT_DIR}"
    "${RSMI_INC_DIR}")
# TODO: set the properties for the library once we have one
## Set the VERSION and SOVERSION values
set_property(TARGET ${CLIENT_LIB} PROPERTY
    SOVERSION "${VERSION_MAJOR}")
set_property(TARGET ${CLIENT_LIB} PROPERTY
    VERSION "${SO_VERSION_STRING}")

## If the library is a release, strip the target library
if("${CMAKE_BUILD_TYPE}" STREQUAL Release)
    add_custom_command(
        TARGET ${CLIENT_LIB}
        POST_BUILD COMMAND ${CMAKE_STRIP} lib${CLIENT_LIB}.so)
endif()

## Add the install directives for the runtime library.
install(TARGETS ${CLIENT_LIB}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/${RDC}
    COMPONENT ${CLIENT_COMPONENT})
install(DIRECTORY ${PROJECT_SOURCE_DIR}/authentication
    DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/${RDC}
    COMPONENT ${CLIENT_COMPONENT})

message("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&")
message("                    Finished Cmake Client Lib                      ")
message("&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&&")
