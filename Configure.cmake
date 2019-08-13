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

#SET(CMAKE_MACOSX_RPATH ON)
#SET(CMAKE_SKIP_BUILD_RPATH FALSE)
#SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
#SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
#SET(CMAKE_INSTALL_RPATH_USE_LINK_RPATH FALSE)

SET(CMAKE_C_STANDARD 11)
SET(CMAKE_C_STANDARD_REQUIRED TRUE)

# Threads
FIND_PACKAGE(Threads)

# Memory
IF(YELLA_FREEBSD)
    SET(YELLA_MEM_LIBS umem)
ENDIF()

# Compiler flags
IF(CMAKE_C_COMPILER_ID MATCHES Clang OR CMAKE_COMPILER_IS_GNUCC)
    CHECK_C_COMPILER_FLAG(-fvisibility=hidden YELLA_VIS_FLAG)
    IF(NOT YELLA_VIS_FLAG)
        MESSAGE(FATAL_ERROR "-fvisibility=hidden is required")
    ENDIF()
    SET(YELLA_SO_FLAGS -fvisibility=hidden)
ELSEIF(CMAKE_C_COMPILER_ID STREQUAL SunPro)
    CHECK_C_COMPILER_FLAG(-xc11 YELLA_HAVE_XC11)
    IF(NOT YELLA_HAVE_XC99)
        MESSAGE(FATAL_ERROR "-xc11 is required")
    ENDIF()
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -xc11")
    CHECK_C_COMPILER_FLAG(-xldscope=hidden YELLA_VIS_FLAG)
    IF(NOT YELLA_VIS_FLAG)
        MESSAGE(FATAL_ERROR "-xldscope=hidden is required")
    ENDIF()
    SET(YELLA_SO_FLAGS -xldscope=hidden)
ENDIF()

# C++
SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Clang leak checks
IF(CMAKE_C_COMPILER_ID MATCHES Clang)
    SET(CMAKE_C_FLAGS_CLANGLEAKS "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address" CACHE STRING "Clang leak checks" FORCE)
ENDIF()

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
FIND_PACKAGE(OpenSSL REQUIRED)

# FlatBuffers
FIND_PROGRAM(YELLA_FLATCC flatcc PATHS "${FLATCC_PATH}")
IF(NOT YELLA_FLATCC)
    MESSAGE(FATAL_ERROR "Set FLATCC_PATH")
ENDIF()
FIND_PATH(YELLA_FLATCC_INCLUDE_DIR flatcc/flatcc_flatbuffers.h PATHS "${FLATCC_INCLUDE_DIR}")
IF(NOT YELLA_FLATCC_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set FLATCC_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_FLATCC_LIB flatccrt PATHS "${FLATCC_LIB_DIR}")
IF(NOT YELLA_FLATCC_LIB)
    MESSAGE(FATAL_ERROR "Set FLATCC_LIB_DIR")
ENDIF()
FIND_PATH(YELLA_FLATBUFFERS_INCLUDE_DIR flatbuffers/flatbuffers.h PATHS "${FLATBUFFERS_INCLUDE_DIR}")
IF(NOT YELLA_FLATCC_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set FLATBUFFERS_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_FLATBUFFERS_LIB flatbuffers PATHS "${FLATBUFFERS_LIB_DIR}")
IF(NOT YELLA_FLATBUFFERS_LIB)
    MESSAGE(FATAL_ERROR "Set FLATBUFFERS_LIB_DIR")
ENDIF()
FIND_PROGRAM(YELLA_FLATC flatc PATHS "${FLATC_PATH}")
IF(NOT YELLA_FLATC)
    MESSAGE(FATAL_ERROR "Set FLATC_PATH")
ENDIF()

# YAML
FIND_PATH(YELLA_LIBYAML_INCLUDE_DIR yaml.h PATHS "${LIBYAML_INCLUDE_DIR}")
IF(NOT YELLA_LIBYAML_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set LIBYAML_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_LIBYAML_LIB yaml PATHS "${LIBYAML_LIB_DIR}")
IF(NOT YELLA_LIBYAML_LIB)
    MESSAGE(FATAL_ERROR "Set LIBYAML_LIB_DIR")
ENDIF()
FIND_PATH(YELLA_YAML_CPP_INCLUDE_DIR yaml-cpp/yaml.h PATHS "${YAML_CPP_INCLUDE_DIR}")
IF(NOT YELLA_YAML_CPP_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set YAML_CPP_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_YAML_CPP_LIB yaml-cpp PATHS "${YAML_CPP_LIB_DIR}")
IF(NOT YELLA_YAML_CPP_LIB)
    MESSAGE(FATAL_ERROR "Set YAML_CPP_LIB_DIR")
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
FIND_PATH(YELLA_CHUCHO_STATIC_INCLUDE_DIR chucho/log.hpp PATHS "${CHUCHO_STATIC_INCLUDE_DIR}" NO_DEFAULT_PATH)
IF(NOT YELLA_CHUCHO_STATIC_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set CHUCHO_STATIC_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_CHUCHO_STATIC_LIB libchucho.a PATHS "${CHUCHO_STATIC_LIB_DIR}")
IF(NOT YELLA_CHUCHO_STATIC_LIB)
    MESSAGE(FATAL_ERROR "Set CHUCHO_STATIC_LIB_DIR")
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

# cJSON
FIND_PATH(YELLA_CJSON_INCLUDE_DIR cjson/cJSON.h PATHS "${CJSON_INCLUDE_DIR}")
IF(NOT YELLA_CJSON_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set CJSON_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_CJSON_LIB cjson PATHS "${CJSON_LIB_DIR}")
IF(NOT YELLA_CJSON_LIB)
    MESSAGE(FATAL_ERROR "Set CJSON_LIB_DIR")
ENDIF()

# sqlite3
FIND_PATH(YELLA_SQLITE3_INCLUDE_DIR sqlite3.h PATHS "${SQLITE3_INCLUDE_DIR}")
IF(NOT YELLA_SQLITE3_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set SQLITE3_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_SQLITE3_LIB sqlite3 PATHS "${SQLITE3_LIB_DIR}")
IF(NOT YELLA_SQLITE3_LIB)
    MESSAGE(FATAL_ERROR "Set SQLITE3_LIB_DIR")
ENDIF()

# Apple frameworks
IF(YELLA_MACINTOSH)
    FIND_LIBRARY(YELLA_CORE_FOUNDATION CoreFoundation)
    FIND_LIBRARY(YELLA_IO_KIT IOKit)
ENDIF()

# ICU
FIND_PACKAGE(ICU REQUIRED COMPONENTS uc i18n io data)

# libfswatch
IF(YELLA_LINUX OR YELLA_MACINTOSH OR YELLA_LINUX)
    SET(YELLA_LIBFSWATCH TRUE)
ENDIF()
IF(YELLA_LIBFSWATCH)
    FIND_PATH(YELLA_LIBFSWATCH_INCLUDE_DIR libfswatch/c/libfswatch.h PATHS "${LIBFSWATCH_INCLUDE_DIR}")
    IF(NOT YELLA_LIBFSWATCH_INCLUDE_DIR)
        MESSAGE(FATAL_ERROR "Set LIBFSWATCH_INCLUDE_DIR")
    ENDIF()
    FIND_LIBRARY(YELLA_LIBFSWATCH_LIB fswatch PATHS "${LIBFSWATCH_LIB_DIR}")
    IF(NOT YELLA_LIBFSWATCH_LIB)
        MESSAGE(FATAL_ERROR "Set LIBFSWATCH_LIB_DIR")
    ENDIF()
ENDIF()

# RabbitMQ
FIND_PATH(YELLA_RABBITMQ_INCLUDE_DIR amqp.h PATHS "${RABBITMQ_INCLUDE_DIR}")
IF(NOT YELLA_RABBITMQ_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set RABBITMQ_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_RABBITMQ_LIB rabbitmq PATHS "${RABBITMQ_LIB_DIR}")
IF(NOT YELLA_RABBITMQ_LIB)
    MESSAGE(FATAL_ERROR "Set RABBITMQ_LIB_DIR")
ENDIF()

# Qt
FIND_PACKAGE(Qt4 REQUIRED COMPONENTS QtGui)

# Postgres
FIND_PATH(YELLA_POSTGRES_INCLUDE_DIR pqxx/pqxx PATHS "${POSTGRES_INCLUDE_DIR}")
IF(NOT YELLA_POSTGRES_INCLUDE_DIR)
    MESSAGE(FATAL_ERROR "Set POSTGRES_INCLUDE_DIR")
ENDIF()
FIND_LIBRARY(YELLA_POSTGRES_LIB pq PATHS "${POSTGRES_LIB_DIR}")
IF(NOT YELLA_POSTGRES_LIB)
    MESSAGE(FATAL_ERROR "Set POSTGRES_LIB_DIR")
ENDIF()
FIND_LIBRARY(YELLA_POSTGRESXX_LIB pqxx PATHS "${POSTGRESXX_LIB_DIR}")
IF(NOT YELLA_POSTGRESXX_LIB)
    MESSAGE(FATAL_ERROR "Set POSTGRESXX_LIB_DIR")
ENDIF()
