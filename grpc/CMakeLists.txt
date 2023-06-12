# Copyright 2018 gRPC authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# cmake build file for C++ helloworld example.
# Assumes protobuf and gRPC have been installed using cmake.
# See cmake_externalproject/CMakeLists.txt for all-in-one cmake build
# that automatically builds all the dependencies before building helloworld.

cmake_minimum_required(VERSION 3.8)

project(WhisperStreaming C CXX)

include(./common.cmake)

set(proto_name "whisper_streaming")
# Proto file
get_filename_component(myshell_streaming_proto "./proto/${proto_name}.proto" ABSOLUTE)
get_filename_component(myshell_streaming_proto_path "${myshell_streaming_proto}" PATH)

# Generated sources
set(myshell_streaming_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.pb.cc")
set(myshell_streaming_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.pb.h")
set(myshell_streaming_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.cc")
set(myshell_streaming_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/${proto_name}.grpc.pb.h")
add_custom_command(
      OUTPUT "${myshell_streaming_proto_srcs}" "${myshell_streaming_proto_hdrs}" "${myshell_streaming_grpc_srcs}" "${myshell_streaming_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${myshell_streaming_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${myshell_streaming_proto}"
      DEPENDS "${myshell_streaming_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# myshell_streaming_grpc_proto
add_library(myshell_streaming_proto
  ${myshell_streaming_grpc_srcs}
  ${myshell_streaming_grpc_hdrs}
  ${myshell_streaming_proto_srcs}
  ${myshell_streaming_proto_hdrs})
target_link_libraries(myshell_streaming_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# Targets greeter_[async_](client|server)
foreach(_target
  whisper_streaming_client whisper_streaming_server)
  add_executable(${_target} "${_target}.cc")
  target_link_libraries(${_target}
    myshell_streaming_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()
