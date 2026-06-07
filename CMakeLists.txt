#添加线程池库
cmake_minimum_required(VERSION 3.28.3)
project(test)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
add_library(threadpool_lib STATIC threadpool.cpp threadpool.h)