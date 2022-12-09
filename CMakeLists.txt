cmake_minimum_required(VERSION 3.24)
project(allocator_for_memory C)

set(CMAKE_C_STANDARD 17)

add_executable(allocator_for_memory main.c src/util.c src/mem_debug.c src/mem.c src/test.c)
