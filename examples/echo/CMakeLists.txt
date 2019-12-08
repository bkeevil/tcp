# CMakeLists.txt
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(echoserver VERSION 0.1 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(BOOST_LIBRARYDIR "/usr/lib/x86_64-linux-gnu")
set(OPENSSL_ROOT_DIR "/usr/lib/x86_64-linux-gnu")
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

find_package(Threads)
find_package(Boost 1.5 COMPONENTS system filesystem program_options REQUIRED)
find_package(OpenSSL REQUIRED)
message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")

find_package (Threads)

include_directories(
  ${Boost_INCLUDE_DIR}
  ${OPENSSL_INCLUDE_DIR}
)
add_executable(echoserver 
  echoserver.cpp
  server.cpp 
)

add_executable(echoclient
  echoclient.cpp
  clientoptions.cpp
  client.cpp
)

target_link_libraries(echoclient tcp)
target_link_libraries(echoclient rt)
target_link_libraries(echoclient ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(echoclient ${OPENSSL_LIBRARIES})
target_link_libraries(echoclient -static-libgcc -static-libstdc++ ${Boost_LIBRARIES})

target_link_libraries(echoserver tcp)
target_link_libraries(echoserver rt)
target_link_libraries(echoserver ${CMAKE_THREAD_LIBS_INIT})
target_link_libraries(echoserver ${OPENSSL_LIBRARIES})
target_link_libraries(echoserver -static-libgcc -static-libstdc++ ${Boost_LIBRARIES})
