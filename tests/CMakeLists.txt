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
target_link_libraries(compile_test pthread)
add_test(compile compile_test)

add_executable(dump_sizeof dump_sizeof.cpp)

macro (do_test name)
  set(testname ${name}_test)
  add_executable(${testname} ${testname}.cpp)
  add_dependencies(${testname} googletest)
  target_link_libraries(${testname} pthread gtest gtest_main)
  add_test(${name} ${testname})
endmacro (do_test)

do_test(basic)
do_test(checked)
do_test(fairness)

