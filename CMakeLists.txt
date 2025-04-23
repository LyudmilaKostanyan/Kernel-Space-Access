cmake_minimum_required(VERSION 3.10)

project(row-vs-colomn)

set(CMAKE_CXX_STANDARD 23)

add_executable(main main.cpp)

if(MSYS)
    target_compile_definitions(main PRIVATE _WIN32=1)
endif()
