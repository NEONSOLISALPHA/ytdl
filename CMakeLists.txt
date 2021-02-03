cmake_minimum_required(VERSION 3.13)

project(ytdl)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-O3")
add_executable(ytdl ytdl.cpp)