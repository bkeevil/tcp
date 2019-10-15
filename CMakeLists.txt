# CMakeLists.txt
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(tcpserver VERSION 0.1 LANGUAGES CXX)

include_directories(include)
add_library(tcpserverlib src/tcp.cpp)
add_executable(tcpserver src/main.cpp)
target_link_libraries(tcpserver tcpserverlib)