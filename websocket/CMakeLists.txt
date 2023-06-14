cmake_minimum_required(VERSION 3.8)

project(WhisperStreaming C CXX)


set(current_directory ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(parent_directory ${current_directory} DIRECTORY)


include_directories("${CMAKE_CURRENT_BINARY_DIR}")

include(DefaultTargetOptions)

file (GLOB SOURCE_FILES *.cc)
file (GLOB HEADER_FILES *.hpp)
set(CMAKE_REQUIRED_LINK_OPTIONS "-arch;arm64")

find_package(websocketpp REQUIRED)
find_package (Boost REQUIRED)
init_target (streaming_websocket_server)

build_executable (${TARGET_NAME} ${SOURCE_FILES} ${HEADER_FILES})
target_link_libraries(${TARGET_NAME} websocketpp::websocketpp)

link_boost ()
final_target ()
