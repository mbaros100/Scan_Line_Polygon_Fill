cmake_minimum_required(VERSION 3.6)
project(ScanLine_Debug)

set(CMAKE_C_STANDARD 11)

set(SOURCE_FILES main.c struct.h)
add_executable(ScanLine_Debug ${SOURCE_FILES})

target_link_libraries(ScanLine_Debug -lX11)