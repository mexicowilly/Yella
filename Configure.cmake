#
# Copyright 2016 Will Mason
# 
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
# 
#      http://www.apache.org/licenses/LICENSE-2.0
# 
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

INCLUDE(CheckCCompilerFlag)
INCLUDE(FindOpenSSL)

# Set consistent platform names
IF(CMAKE_SYSTEM_NAME STREQUAL Windows)
    SET(YELLA_WINDOWS TRUE)
ELSEIF(CMAKE_SYSTEM_NAME STREQUAL Linux)
    SET(YELLA_LINUX TRUE)
ELSEIF(CMAKE_SYSTEM_NAME STREQUAL SunOS)
    SET(YELLA_SOLARIS TRUE)
ELSEIF(CMAKE_SYSTEM_NAME STREQUAL FreeBSD)
    SET(YELLA_FREEBSD TRUE)
ELSEIF(CMAKE_SYSTEM_NAME STREQUAL NetBSD)
    SET(YELLA_NETBSD TRUE)
ELSEIF(CMAKE_SYSTEM_NAME STREQUAL OpenBSD)
    SET(YELLA_OPENBSD TRUE)
ELSEIF(CMAKE_SYSTEM_NAME STREQUAL Darwin)
    SET(YELLA_MACINTOSH TRUE)
ELSEIF(CMAKE_SYSTEM_NAME STREQUAL AIX)
    SET(YELLA_AIX TRUE)
ELSEIF(CYGWIN)
    SET(YELLA_CYGWIN TRUE)
ENDIF()
IF(CMAKE_SYSTEM_NAME MATCHES "^.+BSD$")
    SET(YELLA_BSD TRUE)
ENDIF()
IF(NOT YELLA_WINDOWS)
    SET(YELLA_POSIX TRUE)
ENDIF()

# Set default build type
IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Release CACHE STRING "Build type, one of: Release, Debug, RelWithDebInfo, or MinSizeRel" FORCE)
ENDIF()
MESSAGE(STATUS "Build type -- ${CMAKE_BUILD_TYPE}")

# Compiler flags
IF(CMAKE_C_COMPILER_ID STREQUAL Clang OR CMAKE_COMPILER_IS_GNUCC)
    CHECK_C_COMPILER_FLAG(-std=gnu99 YELLA_HAVE_STDGNU99)
    IF(NOT YELLA_HAVE_STDGNU99)
        MESSAGE(FATAL_ERROR "-std=gnu99 is required")
    ENDIF()
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
    CHECK_C_COMPILER_FLAG(-fvisibility=hidden YELLA_VIS_FLAG)
    IF(NOT YELLA_VIS_FLAG)
        MESSAGE(FATAL_ERROR "-fvisibility=hidden is required")
    ENDIF()
    SET(YELLA_SO_FLAGS -fvisibility=hidden)
ELSEIF(CMAKE_C_COMPILER_ID STREQUAL SunPro)
    CHECK_C_COMPILER_FLAG(-xc99 YELLA_HAVE_XC99)
    IF(NOT YELLA_HAVE_XC99)
        MESSAGE(FATAL_ERROR "-xc99 is required")
    ENDIF()
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -xc99")
    CHECK_C_COMPILER_FLAG(-xldscope=hidden YELLA_VIS_FLAG)
    IF(NOT YELLA_VIS_FLAG)
        MESSAGE(FATAL_ERROR "-xldscope=hidden is required")
    ENDIF()
    SET(YELLA_SO_FLAGS -xldscope=hidden)
ENDIF()

FIND_PACKAGE(Threads REQUIRED)

# ZeroMQ
FIND_PATH(YELLA_ZEROMQ_INCLUDE_DIR zmq.h PATHS "${ZEROMQ_INCLUDE_DIR}")
IF(NOT YELLA_ZEROMQ_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set ZEROMQ_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_ZEROMQ_LIB zmq PATHS "${ZEROMQ_LIB_DIR}")
IF(NOT YELLA_ZEROMQ_LIB)
    MESSAGE(FATAL_ERROR "Set ZEROMQ_LIB_DIR")
ENDIF()

# OpenSSL
FIND_PACKAGE(OpenSSL)
IF(NOT OPENSSL_FOUND)
    MESSAGE(FATAL_ERROR "Set OPENSSL_ROOT_DIR to find OpenSSL")
ENDIF()

# FlatBuffers
FIND_PROGRAM(YELLA_FLATCC
             flatcc
             PATHS "${FLATCC_PATH}")
IF(NOT YELLA_FLATCC)
    MESSAGE(FATAL_ERROR "Set FLATCC_PATH")
ENDIF()
FIND_PATH(YELLA_FLATCC_INCLUDE_DIR flatcc/flatcc_builder.h PATHS "${FLATCC_INCLUDE_DIR}")
IF(NOT YELLA_FLATCC_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set FLATCC_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_FLATCC_LIB flatccrt PATHS "${FLATCC_LIB_DIR}")
IF(NOT YELLA_FLATCC_LIB)
    MESSAGE(FATAL_ERROR "Set FLATCC_LIB_DIR")
ENDIF()

# libyaml
FIND_PATH(YELLA_LIBYAML_INCLUDE_DIR yaml.h PATHS "${LIBYAML_INCLUDE_DIR}")
IF(NOT YELLA_LIBYAML_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set LIBYAML_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_LIBYAML_LIB yaml PATHS "${LIBYAML_LIB_DIR}")
IF(NOT YELLA_LIBYAML_LIB)
    MESSAGE(FATAL_ERROR "Set LIBYAML_LIB_DIR")
ENDIF()

# lz4
FIND_PATH(YELLA_LZ4_INCLUDE_DIR lz4.h PATHS "${LZ4_INCLUDE_DIR}")
IF(NOT YELLA_LZ4_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set LZ4_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_LZ4_LIB lz4 PATHS "${LZ4_LIB_DIR}")
IF(NOT YELLA_LZ4_LIB)
    MESSAGE(FATAL_ERROR "Set LZ4_LIB_DIR")
ENDIF()

# Chucho
FIND_PATH(YELLA_CHUCHO_INCLUDE_DIR chucho/log.h PATHS "${CHUCHO_INCLUDE_DIR}")
IF(NOT YELLA_CHUCHO_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set CHUCHO_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_CHUCHO_LIB chucho PATHS "${CHUCHO_LIB_DIR}")
IF(NOT YELLA_CHUCHO_LIB)
    MESSAGE(FATAL_ERROR "Set CHUCHO_LIB_DIR")
ENDIF()

# libuuid
IF(YELLA_POSIX)
    FIND_PATH(YELLA_UUID_INCLUDE_DIR uuid.h PATHS "${UUID_INCLUDE_DIR}" PATH_SUFFIXES uuid)
    IF(NOT YELLA_UUID_INCLUDE_DIR)
        MESSAGE(FATAL_ERROR "Set UUID_INCLUDE_DIR")
    ENDIF()
    FIND_LIBRARY(YELLA_UUID_LIB uuid PATHS "${UUID_LIB_DIR}")
    IF(NOT YELLA_UUID_LIB)
        MESSAGE(FATAL_ERROR "Set UUID_LIB_DIR")
    ENDIF()
ENDIF()

# CMocka
FIND_PATH(YELLA_CMOCKA_INCLUDE_DIR cmocka.h PATHS "${CMOCKA_INCLUDE_DIR}")
IF(NOT YELLA_CMOCKA_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set CMOCKA_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_CMOCKA_LIB cmocka PATHS "${CMOCKA_LIB_DIR}")
IF(NOT YELLA_CMOCKA_LIB)
    MESSAGE(FATAL_ERROR "Set CMOCKA_LIB_DIR")
ENDIF()
