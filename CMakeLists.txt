cmake_minimum_required(VERSION 3.5)
project(zy_socks)

add_definitions(-std=c++11)
add_compile_options(-O0 -g -Wall -Werror)

find_package(Protobuf REQUIRED)
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HEADERS client.proto server.proto)
add_library(proto ${PROTO_SRCS} ${PROTO_HEADERS})
target_link_libraries(proto protobuf)
# include protobuf headers from cmake binary directory
include_directories(${CMAKE_BINARY_DIR})

add_library(json config_json.cc)

link_libraries(
        muduo_net_cpp11
        muduo_base_cpp11
        muduo_cdns
        cares
        pthread
        snappy
        proto
        json
)
include_directories(${CMAKE_SOURCE_DIR})

add_subdirectory(client)
add_subdirectory(server)


