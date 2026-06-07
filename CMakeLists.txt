#添加线程池库
cmake_minimum_required(VERSION 3.28.3 )
project(test LANGUAGES CXX)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)

set(ThreadPool_SOURCES
src/Task_Queue.cpp
src/threadpool.cpp
)

add_subdirectory(logger)
add_library(threadpool_lib STATIC ${ThreadPool_SOURCES})

target_link_libraries(threadpool_lib PUBLIC logger_lib)

# 测试程序用来进行压测
add_executable(threadpool_test threadpool_test.cpp)
target_link_libraries(threadpool_test PUBLIC threadpool_lib)
target_include_directories(threadpool_test PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)