cmake_minimum_required(VERSION 3.10)
project(socks5)

set(CMAKE_CXX_STANDARD 14)
add_subdirectory(config_parser)

add_executable(socks5 main.cpp Protocol.cpp Protocol.h Socks5Server.h Socks5Server.cpp FifoBuffer.h  ServerUtil.cpp ServerUtil.h RawList.h ServerConfig.h ServerConfig.cpp Session.cpp Session.h)
target_link_libraries(socks5 libevent.a)
target_link_libraries(socks5 pthread)
target_link_libraries(socks5 config_parser)
