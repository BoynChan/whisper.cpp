# whisper 小册

## repo

https://github.com/BoynChan/whisper.cpp

## 编译

```bash
# 切记! 一定要在build文件夹下面运行
mkdir -p cmake/build
cd cmake/build
cmake -DWHISPER_SDL2=1 ../..

make whisper_streaming_server
make whisper_streaming_client
```

## 服务化

grpc 基础服务安装过程: https://grpc.io/docs/languages/cpp/quickstart/

```bash
export MY_INSTALL_DIR=$HOME/.local
mkdir -p $MY_INSTALL_DIR
export PATH="$MY_INSTALL_DIR/bin:$PATH"
sudo apt install -y cmake // 注意版本问题
sudo apt install -y build-essential autoconf libtool pkg-config
git clone --recurse-submodules -b v1.55.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc
cd grpc
mkdir -p cmake/build
cd cmake/build
cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
      ../..
make -j 4
make install
```

### 例子

```
cd examples/cpp/helloworld
mkdir -p cmake/build
cd cmake/build
cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ../..
make -j
```
