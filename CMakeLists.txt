cmake_minimum_required(VERSION 2.8)
project(zy_socks)

add_definitions(-std=c++11)
add_compile_options(-O2 -g -Wall -Werror)

find_package(Protobuf REQUIRED)
PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HEADERS client.proto server.proto)
add_library(proto ${PROTO_SRCS} ${PROTO_HEADERS})

find_library(PROTOBUF libprotobuf.a REQUIRED)

target_link_libraries(proto ${PROTOBUF})
# include protobuf headers from cmake binary directory
include_directories(${CMAKE_BINARY_DIR})

add_library(json config_json.cc)

find_library(CARES libcares.a REQUIRED)
find_library(SNAPPY libsnappy.a REQUIRED)

link_libraries(
        muduo_net_cpp11
        muduo_base_cpp11
        muduo_cdns
        pthread
        proto
        json
        ${SNAPPY}
        ${CARES}
)

include_directories(${CMAKE_SOURCE_DIR})

add_subdirectory(client)
add_subdirectory(server)
