cmake_minimum_required(VERSION 3.10)
project(dbn LANGUAGES C)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_compile_options(-Werror -Wall)

add_subdirectory(dbn)
add_subdirectory(dbn_stats)
add_subdirectory(dbn_roots)

