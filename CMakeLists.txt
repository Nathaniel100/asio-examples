cmake_minimum_required(VERSION 3.6.0)
project(asio-examples)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_path(ASIO_INCLUDE_DIR asio)
if (ASIO_INCLUDE_DIR STREQUAL "ASIO_INCLUDE_DIR-NOTFOUND")
  message(FATAL_ERROR "Could not find asio library")
endif()
message(STATUS "Found: ${ASIO_INCLUDE_DIR}")
include_directories(${ASIO_INCLUDE_DIR})

add_subdirectory(chat)
