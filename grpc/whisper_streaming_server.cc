/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <thread>
#include <vector>

#include "streaming_common.h"
#include "whisper.h"
#include "whisper_streaming.grpc.pb.h"

using namespace myshell;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

// Logic and data behind the server's behavior.
class WhisperStreamingServer final : public WhisperStreaming::Service {
private:
  whisper_context *ctx;

public:
  WhisperStreamingServer(whisper_context *ctx) { this->ctx = ctx; }

  Status Transcribe(ServerContext *context, const TranscribeRequest *req,
                    TranscribeResponse *reply) override {
    std::vector<float> pcmf32(req->audio_data().begin(),
                              req->audio_data().end());
    std::cout << "ASR audio length:" << pcmf32.size() << std::endl;

    whisper_full_params wparams =
        whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    wparams.print_progress = false;
    wparams.print_special = false;
    wparams.print_realtime = false;
    wparams.print_timestamps = false;
    wparams.translate = false;
    wparams.no_context = true;
    wparams.single_segment = true;
    wparams.language = "en";
    wparams.n_threads =
        std::min(4, (int32_t)std::thread::hardware_concurrency());
    wparams.speed_up = false;

    if (whisper_full(this->ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
      reply->set_message("reconize failed.");
      return Status::CANCELLED;
    }

    float prob = 0.0f;
    int prob_n = 0;
    std::string result;
    const int n_segments = whisper_full_n_segments(ctx);
    for (int i = 0; i < n_segments; ++i) {
      const char *text = whisper_full_get_segment_text(ctx, i);

      result += text;

      const int n_tokens = whisper_full_n_tokens(ctx, i);
      for (int j = 0; j < n_tokens; ++j) {
        const auto token = whisper_full_get_token_data(ctx, i, j);

        prob += token.p;
        ++prob_n;
      }
    }

    if (prob_n > 0) {
      prob /= prob_n;
    }

    reply->set_text(result);
    reply->set_message("success");
    return Status::OK;
  }

  Status Ping(ServerContext *context, const EmptyReq *request,
              PingReply *reply) override {
    reply->set_message("Pong");
    return Status::OK;
  }
};

void init_server(whisper_streaming_params &params) {
  std::cout << "init model:" + params.model << std::endl;

  struct whisper_context *ctx = whisper_init_from_file(params.model.c_str());

  if (ctx == nullptr) {
    fprintf(stderr, "error: failed to initialize whisper context\n");
    exit(3);
  }

  std::string server_address = "0.0.0.0:" + params.port;
  WhisperStreamingServer service(ctx);

  grpc::EnableDefaultHealthCheckService(true);
  grpc::reflection::InitProtoReflectionServerBuilderPlugin();
  ServerBuilder builder;
  builder.SetMaxMessageSize(10 * 1024 * 1024);
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on:" << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char **argv) {
  whisper_streaming_params params;

  if (!whisper_streaming_params_parse(argc, argv, params)) {
    return 1;
  }
  init_server(params);
  return 0;
}
