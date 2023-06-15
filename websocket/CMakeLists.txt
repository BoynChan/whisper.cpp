cmake_minimum_required(VERSION 3.8)

project(WhisperStreaming C CXX)


set(current_directory ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(parent_directory ${current_directory} DIRECTORY)

set(current_directory ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(parent_directory ${current_directory} DIRECTORY)

include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${parent_directory}/examples")

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

include(DefaultTargetOptions)

file (GLOB SOURCE_FILES *.cc)
file (GLOB HEADER_FILES *.hpp)

find_package(websocketpp REQUIRED)
find_package (Boost REQUIRED COMPONENTS system filesystem)
find_package (Threads REQUIRED)
init_target (streaming_websocket_server)

include_directories(${TARGET_NAME} ${Boost_INCLUDE_DIRS})
build_executable (${TARGET_NAME} ${SOURCE_FILES} ${HEADER_FILES})
target_include_directories(${TARGET_NAME} PRIVATE ${VAD_DIR})
target_link_libraries(${TARGET_NAME} websocketpp::websocketpp common whisper pthread ${Boost_LIBRARIES})

link_boost ()
final_target ()
