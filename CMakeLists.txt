cmake_minimum_required(VERSION 3.10)
project(Kernel-Space-Access)

set(CMAKE_CXX_STANDARD 17)

include_directories(${CMAKE_SOURCE_DIR}/include)

set(SOURCES src/main.cpp)

if (WIN32 AND NOT MINGW)
    list(APPEND SOURCES src/memory_utils_windows.cpp)
else()
    list(APPEND SOURCES src/memory_utils_posix.cpp)
endif()

add_executable(${PROJECT_NAME} ${SOURCES})
