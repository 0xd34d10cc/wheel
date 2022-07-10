cmake_minimum_required(VERSION 3.9 FATAL_ERROR)
project(wheel CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

add_executable(wh
  main.cpp
)

target_include_directories(wh PUBLIC include/)