cmake_minimum_required(VERSION 2.8.4)

add_executable(compile_test compile_test.cpp)
add_executable(dump_sizeof dump_sizeof.cpp)

target_link_libraries(compile_test
    pthread
    )

add_test(compile compile_test)
