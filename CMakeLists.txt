cmake_minimum_required(VERSION 2.8.4)

project(yamc CXX)

set(CMAKE_CXX_FLAGS "-std=c++11 -Wall -Wextra -pedantic -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
set(CMAKE_CXX_FLAGS_RELEASE "-DNDEBUG -O2")

include_directories(include)
enable_testing()

add_subdirectory(tests)
