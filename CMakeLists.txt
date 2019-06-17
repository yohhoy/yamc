cmake_minimum_required(VERSION 3.2)

project(yamc C CXX)

# Requires C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ENABLE_TESTING "Enable unit testing." ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")
endif()

include_directories(include)

if (ENABLE_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

install(DIRECTORY include/ DESTINATION include/yamc/
    FILES_MATCHING PATTERN "*.hpp")
