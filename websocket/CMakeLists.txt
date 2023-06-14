cmake_minimum_required(VERSION 3.8)

project(WhisperStreaming C CXX)


set(current_directory ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(parent_directory ${current_directory} DIRECTORY)


include_directories("${CMAKE_CURRENT_BINARY_DIR}")

include(DefaultTargetOptions)

add_executable(websocket_server "websocket_server.cc")
target_link_libraries(websocket_server
    common
    whisper 
    ${CMAKE_THREAD_LIBS_INIT})
