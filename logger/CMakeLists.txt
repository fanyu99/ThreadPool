#添加日志库
cmake_minimum_required(VERSION 3.28.3)
project(Logger LANGUAGES CXX)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
add_library(logger_lib STATIC src/logger.cpp)
target_include_directories(logger_lib PUBLIC include)