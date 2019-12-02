# CMakeLists.txt
cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
project(tcpserver VERSION 0.1 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

#set(BOOST_LIBRARYDIR "/usr/lib/x86_64-linux-gnu")
set(OPENSSL_ROOT_DIR "/usr/lib/x86_64-linux-gnu")
set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

#find_package(Threads)
#find_package(Boost 1.65 COMPONENTS program_options REQUIRED)
find_package(OpenSSL REQUIRED)
message("OpenSSL include dir: ${OPENSSL_INCLUDE_DIR}")
message("OpenSSL libraries: ${OPENSSL_LIBRARIES}")

#enable_testing()

include_directories(
#  ${Boost_INCLUDE_DIR}
  ${OPENSSL_INCLUDE_DIR}
  include
)


add_subdirectory(examples/echo)

#add_subdirectory(tests/driver)

add_library(tcp 
  src/tcpsocket.cpp
  src/tcpclient.cpp
  src/tcpserver.cpp
)

#target_link_libraries(tcp rt)
#target_link_libraries(tcp ${CMAKE_THREAD_LIBS_INIT} )
#target_link_libraries(tcp -static-libgcc -static-libstdc++ ${Boost_LIBRARIES} )
target_link_libraries(tcp ${OPENSSL_LIBRARIES} )

#add_test(NAME createServer  COMMAND tcptestdriver createServer)
#add_test(NAME createClient  COMMAND tcptestdriver createClient)
#add_test(NAME destroyClient COMMAND tcptestdriver destroyClient)
#add_test(NAME destroyServer COMMAND tcptestdriver destroyServer)
