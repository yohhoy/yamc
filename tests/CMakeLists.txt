cmake_minimum_required(VERSION 3.2)

# Pthread
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

# Google Test
if(MSVC)
  set(GTEST_FORCE_SHARED_CRT ON)
endif()
include(ExternalProject)
set(EXTERNAL_INSTALL_LOCATION ${CMAKE_BINARY_DIR}/external)
ExternalProject_Add(googletest
  GIT_REPOSITORY https://github.com/google/googletest
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=${EXTERNAL_INSTALL_LOCATION}
             -Dgtest_force_shared_crt=${GTEST_FORCE_SHARED_CRT}
)

include_directories(${EXTERNAL_INSTALL_LOCATION}/include)
link_directories(${EXTERNAL_INSTALL_LOCATION}/lib)

add_executable(compile_test compile_test.cpp)
target_link_libraries(compile_test Threads::Threads)
add_test(compile compile_test)

add_executable(dump_sizeof dump_typeinfo.cpp)
add_executable(dump_layout dump_typeinfo.cpp)
set_target_properties(dump_sizeof PROPERTIES COMPILE_DEFINITIONS "DUMP_SIZEOF")
set_target_properties(dump_layout PROPERTIES COMPILE_DEFINITIONS "DUMP_STANDARD_LAYOUT")

macro (do_test name)
  set(testname ${name}_test)
  add_executable(${testname} ${testname}.cpp)
  add_dependencies(${testname} googletest)
  target_link_libraries(${testname} Threads::Threads gtest gtest_main)
  add_test(${name} ${testname})
endmacro (do_test)

do_test(basic)
do_test(checked)
do_test(fairness)
do_test(rwlock)
