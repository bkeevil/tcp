# CMakeLists.txt
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(echoserver VERSION 0.1 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

find_package (Threads)

add_executable(echoserver 
  echoserver.cpp
  server.cpp 
)

add_executable(echoclient
  echoclient.cpp
  client.cpp
)

target_link_libraries(echoserver tcp)
target_link_libraries(echoclient tcp)
target_link_libraries (echoclient ${CMAKE_THREAD_LIBS_INIT})