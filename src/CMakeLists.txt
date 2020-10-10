cmake_minimum_required(VERSION 3.10)
project(socks5Server)

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(config_parser)
add_executable(socks5Server main.cpp Socks5Server.cpp Socks5Server.h Protocol.cpp Protocol.h RawList.h ServerUtil.cpp ServerUtil.h FifoBuffer.h ServerConfigs.h ServerConfigs.cpp)
target_link_libraries(socks5Server libevent.a)
target_link_libraries(socks5Server pthread)
target_link_libraries(socks5Server config_parser)
