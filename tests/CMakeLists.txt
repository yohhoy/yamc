cmake_minimum_required(VERSION 2.8.4)

include(ExternalProject)

set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)

ExternalProject_Add(googletest
    GIT_REPOSITORY https://github.com/google/googletest
    CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
)

include_directories(${EXTERNAL_INSTALL_LOCATION}/include)
link_directories(${EXTERNAL_INSTALL_LOCATION}/lib)

add_executable(compile_test compile_test.cpp)
add_executable(checked_test checked_test.cpp)
add_executable(basic_test basic_test.cpp)
add_executable(dump_sizeof dump_sizeof.cpp)

add_dependencies(checked_test googletest)
add_dependencies(basic_test googletest)
target_link_libraries(compile_test pthread)
target_link_libraries(checked_test pthread gtest gtest_main)
target_link_libraries(basic_test pthread gtest gtest_main)

add_test(compile compile_test)
add_test(checked checked_test)
add_test(basic basic_test)
