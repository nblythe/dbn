cmake_minimum_required(VERSION 3.10)
project(dbn LANGUAGES C)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_compile_options(-Werror -Wall)
add_compile_options(-Wno-unused-function)

add_subdirectory(libdbn)
add_subdirectory(libdbnopra)
add_subdirectory(dbn_stats)
add_subdirectory(dbn_multi_stats)
add_subdirectory(dbn_roots)
