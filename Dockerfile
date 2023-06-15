# 使用带C++编译环境的基础镜像
FROM debian:buster

# 安装CMake供建设工具和基本的库文件
RUN apt-get update && \
    apt-get install -y cmake make autoconf libtool pkg-config libboost-all-dev build-essential zlib1g wget
# 设置工作目录
WORKDIR /build

# 将CMake项目文件（CMakeLists.txt）和源代码复制到工作目录
COPY ./ /build/
RUN mkdir -p third_party/websocketpp/build && \
    cd third_party/websocketpp/build && \
    cmake .. && \
    make install

WORKDIR /build
# 运行CMake构建
RUN cmake . && make streaming_websocket_server

RUN rm models/*.bin && \
    bash ./models/download-ggml-model.sh base.en && \
    make quantize && \
    ./bin/quantize models/ggml-base.en.bin models/ggml-base.en-q5_0.bin q5_0

# 设置应用程序入口点
# ENTRYPOINT ["/build/bin/streaming_websocket_server"]