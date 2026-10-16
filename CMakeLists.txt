cmake_minimum_required(VERSION 3.20)
project(goldbox_engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GOLDBOX_BUILD_TESTS "Build project tests" ON)

add_subdirectory(formats)
add_subdirectory(engine)
option(GOLDBOX_BUILD_RENDERER "Build the optional SDL3 exploration window" OFF)
if(GOLDBOX_BUILD_RENDERER)
    find_package(SDL3 3.2 CONFIG REQUIRED)
    add_subdirectory(runtime)
endif()
add_subdirectory(apps)

if(GOLDBOX_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
