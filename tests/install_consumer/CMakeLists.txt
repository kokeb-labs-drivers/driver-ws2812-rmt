# A project outside the source tree that uses only the installed package:
# find_package(ws2812_rmt) and ws2812_rmt::ws2812_rmt. Run by tests/check_install.cmake.
cmake_minimum_required(VERSION 3.16)
project(ws2812_rmt_install_consumer LANGUAGES CXX)

# A consumer that builds as ISO C++20 with -pedantic-errors: the installed headers need no extension.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ws2812_rmt 0.1 REQUIRED CONFIG)

add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE ws2812_rmt::ws2812_rmt)
target_compile_options(consumer PRIVATE -fno-exceptions -fno-rtti -pedantic-errors)
