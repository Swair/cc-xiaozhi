cmake_minimum_required(VERSION 3.22)
project(xiaozhiai)


set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -Wall -fPIC")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -std=c++17")


add_subdirectory(cill)
add_subdirectory(xiaozhiai)


