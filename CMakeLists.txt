#添加线程池库
cmake_minimum_required(VERSION 3.28.3 )
project(test LANGUAGES CXX)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)

add_subdirectory(logger)

add_library(threadpool_lib STATIC src/threadpool.cpp)
target_include_directories(threadpool_lib PUBLIC include)
target_link_libraries(threadpool_lib PUBLIC logger_lib)

add_executable(threadpool_test threadpool_test.cpp)
target_link_libraries(threadpool_test PUBLIC threadpool_lib)
