cmake_minimum_required(VERSION 3.20)

project(Kite LANGUAGES CXX)

include(CTest)

add_library(kite_core STATIC
    src/bytecode/bytecode.cpp
    src/interpreter/interpreter.cpp
    src/lexer/lexer.cpp
    src/lexer/token.cpp
    src/parser/parser.cpp
)

target_compile_features(kite_core PUBLIC cxx_std_20)

target_include_directories(kite_core
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

add_executable(kite
    src/main.cpp
)

target_link_libraries(kite PRIVATE kite_core)

if(BUILD_TESTING)
    add_executable(kite_lexer_tests
        tests/lexer/lexer_tests.cpp
    )

    add_executable(kite_parser_tests
        tests/parser/parser_tests.cpp
    )

    add_executable(kite_interpreter_tests
        tests/interpreter/interpreter_tests.cpp
    )

    add_executable(kite_bytecode_tests
        tests/bytecode/bytecode_tests.cpp
    )

    add_executable(kite_stdlib_tests
        tests/stdlib/stdlib_tests.cpp
    )

    target_link_libraries(kite_lexer_tests PRIVATE kite_core)
    target_link_libraries(kite_parser_tests PRIVATE kite_core)
    target_link_libraries(kite_interpreter_tests PRIVATE kite_core)
    target_link_libraries(kite_bytecode_tests PRIVATE kite_core)
    target_link_libraries(kite_stdlib_tests PRIVATE kite_core)
    target_compile_definitions(kite_stdlib_tests PRIVATE KITE_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

    add_test(NAME kite_lexer_tests COMMAND kite_lexer_tests)
    add_test(NAME kite_parser_tests COMMAND kite_parser_tests)
    add_test(NAME kite_interpreter_tests COMMAND kite_interpreter_tests)
    add_test(NAME kite_bytecode_tests COMMAND kite_bytecode_tests)
    add_test(NAME kite_stdlib_tests COMMAND kite_stdlib_tests)
endif()
