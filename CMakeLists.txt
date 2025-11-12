cmake_minimum_required(VERSION 4.0)
project(asynctest)

set(CMAKE_CXX_STANDARD 23)
find_package(cppcoro)
add_executable(asynctest main.cpp
        simpleasync.h)
target_link_libraries(asynctest cppcoro::cppcoro)