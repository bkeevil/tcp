# CMakeLists.txt
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(tcptestdriver VERSION 0.1 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

include_directories(../../examples/echo)

add_executable(tcptestdriver 
  ../../examples/echo/echoclient.cpp
  ../../examples/echo/echoserver.cpp
  main.cpp 
)

target_link_libraries(tcptestdriver tcp)