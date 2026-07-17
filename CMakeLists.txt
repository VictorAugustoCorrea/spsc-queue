cmake_minimum_required(VERSION 4.2)
project(LockFreeQueue)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(LockFreeQueue
        src/benchmark.cpp
        src/SPSC_queue.h)
