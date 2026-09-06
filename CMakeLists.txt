cmake_minimum_required(VERSION 3.20)

project(Kite LANGUAGES CXX)

add_executable(kite
    src/main.cpp
)

target_compile_features(kite PRIVATE cxx_std_20)
