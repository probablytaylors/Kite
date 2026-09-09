cmake_minimum_required(VERSION 3.20)

project(Kite LANGUAGES CXX)

option(KITE_WERROR "Treat compiler warnings as errors" OFF)

add_library(kite_core STATIC
    src/bytecode/bytecode.cpp
    src/interpreter/interpreter.cpp
    src/lexer/lexer.cpp
    src/lexer/token.cpp
    src/parser/parser.cpp
    src/semantic/semantic.cpp
)

target_compile_features(kite_core PUBLIC cxx_std_20)
target_include_directories(kite_core PUBLIC ${PROJECT_SOURCE_DIR}/include)

if(MSVC)
    target_compile_options(kite_core PUBLIC /W4 /permissive-)
    if(KITE_WERROR)
        target_compile_options(kite_core PUBLIC /WX)
    endif()
else()
    target_compile_options(kite_core PUBLIC -Wall -Wextra -Wpedantic -Wshadow)
    if(KITE_WERROR)
        target_compile_options(kite_core PUBLIC -Werror)
    endif()
endif()

add_executable(kite src/main.cpp)
target_link_libraries(kite PRIVATE kite_core)

install(TARGETS kite RUNTIME DESTINATION bin)
