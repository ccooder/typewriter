cmake_minimum_required(VERSION 4.0)
project(nfl_typewriter C)

set(CMAKE_C_STANDARD 23)

aux_source_directory(./application DIR_SRCS)
message(${DIR_SRCS})

add_executable(nfl_typewriter main.c ${DIR_SRCS})

find_package(PkgConfig REQUIRED)
pkg_check_modules(gtk REQUIRED IMPORTED_TARGET gtk4)
target_link_libraries(${PROJECT_NAME} PkgConfig::gtk)