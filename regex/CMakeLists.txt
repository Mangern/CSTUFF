cmake_minimum_required(VERSION 3.0)
project(regex)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(preprocess_test
    preprocess.c
    preprocess_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../da/da.c
)

target_include_directories(preprocess_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../da/)

add_executable(dfa_test
    dfa.c
    nfa.c
    preprocess.c
    dfa_test.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../da/da.c
)

target_include_directories(dfa_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../da/)

add_executable(nfa_test
    nfa.c
    nfa_test.c
    preprocess.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../da/da.c
)

target_include_directories(nfa_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../da/)
