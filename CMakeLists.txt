cmake_minimum_required(VERSION 3.20)

project(Kite VERSION 0.3.2 LANGUAGES CXX)

option(KITE_WERROR "Treat compiler warnings as errors" OFF)

configure_file(
    ${PROJECT_SOURCE_DIR}/cmake/version.hpp.in
    ${PROJECT_BINARY_DIR}/generated/kite/version.hpp
    @ONLY
)

add_library(kite_core STATIC
    src/builtins.cpp
    src/bytecode/bytecode.cpp
    src/interpreter/interpreter.cpp
    src/lexer/lexer.cpp
    src/lexer/token.cpp
    src/module/module.cpp
    src/native/native.cpp
    src/parser/parser.cpp
    src/resolver/resolver.cpp
    src/semantic/semantic.cpp
    src/update.cpp
    src/value.cpp
)

target_compile_features(kite_core PUBLIC cxx_std_20)
target_include_directories(kite_core PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/generated
)

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

set(kite_sources src/main.cpp)
if(WIN32)
    configure_file(
        ${PROJECT_SOURCE_DIR}/cmake/kite.rc.in
        ${PROJECT_BINARY_DIR}/generated/kite.rc
        @ONLY
    )
    list(APPEND kite_sources ${PROJECT_BINARY_DIR}/generated/kite.rc)
endif()

add_executable(kite ${kite_sources})
target_link_libraries(kite PRIVATE kite_core)

if(MSVC)
    target_link_options(kite PRIVATE /STACK:16777216)
endif()

install(TARGETS kite RUNTIME DESTINATION bin)
