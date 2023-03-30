cmake_minimum_required(VERSION 3.24)
project(nfl_typewriter C)

set(CMAKE_C_STANDARD 11)

aux_source_directory(. DIR_SRCS)
add_executable(${PROJECT_NAME} main.c ${DIR_SRCS})

find_package(PkgConfig REQUIRED)
pkg_check_modules(gtk REQUIRED IMPORTED_TARGET gtk4)
target_link_libraries(${PROJECT_NAME} PkgConfig::gtk)

