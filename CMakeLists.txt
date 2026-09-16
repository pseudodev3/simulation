cmake_minimum_required(VERSION 3.20)
project(crime_city_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SIMULATION_BUILD_VIEWER "Build the raylib neighborhood viewer" ON)

add_library(simulation_core
    src/Person.cpp
    src/World.cpp
)

target_include_directories(simulation_core PUBLIC include)

add_executable(simulation src/main.cpp)
target_link_libraries(simulation PRIVATE simulation_core)

if (SIMULATION_BUILD_VIEWER)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(raylib)

    add_executable(simulation_viewer
        src/viewer_main.cpp
        src/NeighborhoodRenderer.cpp
    )
    target_link_libraries(simulation_viewer PRIVATE simulation_core raylib)
    target_include_directories(simulation_viewer PRIVATE include)
endif()

function(enable_project_warnings target)
    if (MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

enable_project_warnings(simulation_core)
enable_project_warnings(simulation)
if (TARGET simulation_viewer)
    enable_project_warnings(simulation_viewer)
endif()
