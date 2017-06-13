project(stream-unrar)
cmake_minimum_required(VERSION 2.8)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-DRARDLL)

add_subdirectory(3rd_party)
include_directories(3rd_party/unrar-4.0.7)

list(APPEND EXE_SRC stream_unrar.cpp)

add_executable(stream-unrar ${EXE_SRC})
target_link_libraries(stream-unrar unrar)
