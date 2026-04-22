cmake_minimum_required(VERSION 3.16)
project(memory_allocators_coursework LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(memory_allocators
    src/allocator.cpp
    src/benchmark.cpp
)

target_include_directories(memory_allocators PRIVATE src)

if(MSVC)
    target_compile_options(memory_allocators PRIVATE /W4)
else()
    target_compile_options(memory_allocators PRIVATE -Wall -Wextra -Wpedantic)
endif()
