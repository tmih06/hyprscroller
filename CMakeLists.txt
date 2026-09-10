cmake_minimum_required(VERSION 3.16)
project("hyprscroller"
    LANGUAGES CXX
    DESCRIPTION "Hyprland scroller layout with dynamic column resizing")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(HYPRLAND REQUIRED hyprland)
pkg_check_modules(DEPS REQUIRED hyprutils hyprlang pixman-1 libdrm pangocairo)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED true)

add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options("-fno-gnu-unique")
endif()

add_library(hyprscroller SHARED
    src/main.cpp
)

target_include_directories(hyprscroller PRIVATE
    ${HYPRLAND_INCLUDE_DIRS}
    ${DEPS_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(hyprscroller PROPERTIES PREFIX "")
