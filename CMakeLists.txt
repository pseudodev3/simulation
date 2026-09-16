cmake_minimum_required(VERSION 3.20)
project(crime_city_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(simulation
    src/main.cpp
    src/Person.cpp
    src/World.cpp
)

target_include_directories(simulation PRIVATE include)

if (MSVC)
    target_compile_options(simulation PRIVATE /W4 /permissive-)
else()
    target_compile_options(simulation PRIVATE -Wall -Wextra -Wpedantic)
endif()
