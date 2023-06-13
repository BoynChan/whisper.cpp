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

#include "common.h"
#include "streaming_common.h"
#include "whisper.h"
#include "whisper_streaming.grpc.pb.h"

using namespace myshell;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReaderWriter;
using grpc::Status;

// Logic and data behind the server's behavior.
class WhisperStreamingServer final : public WhisperStreaming::Service {
private:
  std::vector<char> model_buffer;

public:
  WhisperStreamingServer(std::string model_path) {
    std::ifstream file(model_path, std::ifstream::binary);
    if (!file) {
      std::cerr << "Error opening the file" << std::endl;
      std::cout << "Error opening the file" << std::endl;
      exit(1);
    }
    file.seekg(0, file.end);
    std::streamsize file_size = file.tellg();
    file.seekg(0, file.beg);

    this->model_buffer.resize(file_size);
    if (!file.read(this->model_buffer.data(), file_size)) {
      std::cerr << "Error reading the file" << std::endl;
      std::cout << "Error reading the file" << std::endl;
      exit(1);
    }
    std::cout << "model size: " << this->model_buffer.size() / 1024 / 1024
              << " MB" << std::endl;
  }

  whisper_full_params default_whisper_params() {

    whisper_full_params wparams =
        whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    wparams.print_progress = false;
    wparams.print_special = false;
    wparams.print_realtime = false;
    wparams.print_timestamps = true;
    wparams.translate = false;
    wparams.single_segment = true;
    wparams.max_tokens = 32;
    wparams.language = "en";
    wparams.n_threads =
        std::min(4, (int32_t)std::thread::hardware_concurrency());
    ;

    wparams.audio_ctx = 0;
    wparams.speed_up = false;

    // disable temperature fallback
    // wparams.temperature_inc  = -1.0f;
    wparams.temperature_inc = 0.0f;

    wparams.prompt_tokens = nullptr;
    wparams.prompt_n_tokens = 0;
    return wparams;
  }

  Status Transcribe(ServerContext *context, const TranscribeRequest *req,
                    TranscribeResponse *reply) override {
    std::vector<float> pcmf32(req->audio_data().begin(),
                              req->audio_data().end());
    std::cout << "ASR audio length:" << pcmf32.size() << std::endl;

    struct whisper_context *ctx = whisper_init_from_buffer(
        this->model_buffer.data(), this->model_buffer.size());

    whisper_full_params wparams = this->default_whisper_params();
    if (whisper_full_parallel(ctx, wparams, pcmf32.data(), pcmf32.size(),
                              wparams.n_threads) != 0) {
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

  Status TranscribeStreaming(
      ServerContext *context,
      ServerReaderWriter<StreamingTranscibeReply, StreamingTranscribeRequest>
          *stream) override {
    // ready for whisper params
    const int step_ms = 500;     // each package will have 500 ms
    const int length_ms = 10000; // a sentence will have 10s
    const int keep_ms = 500;     // keep 200ms audio
    const int n_samples_step = (1e-3 * step_ms) * WHISPER_SAMPLE_RATE;
    const int n_samples_len = (1e-3 * length_ms) * WHISPER_SAMPLE_RATE;
    const int n_samples_keep = (1e-3 * keep_ms) * WHISPER_SAMPLE_RATE;
    const int n_samples_30s = (1e-3 * 30000.0) * WHISPER_SAMPLE_RATE;
    const int n_new_line = std::max(
        1, length_ms / step_ms - 1); // number of steps to print new line
    int n_iter = 0;

    struct whisper_context *ctx = whisper_init_from_buffer(
        this->model_buffer.data(), this->model_buffer.size());

    std::vector<float> pcmf32(n_samples_30s, 0.0f);
    std::vector<float> pcmf32_old;

    StreamingTranscribeRequest request;

    // result collection
    std::vector<std::string> whisper_result(0);

    // main inference loop
    while (stream->Read(&request)) {

      StreamingTranscibeReply response;
      // step1. get audio data
      std::vector<float> pcmf32_new(request.audio_data().begin(),
                                    request.audio_data().end());

      // step2. prepare audio data for inference
      const int n_samples_new = pcmf32_new.size();
      const int n_samples_take =
          std::min((int)pcmf32_old.size(),
                   std::max(0, n_samples_keep + n_samples_len - n_samples_new));
      pcmf32.resize(n_samples_new + n_samples_take);

      for (int i = 0; i < n_samples_take; i++) {
        pcmf32[i] = pcmf32_old[pcmf32_old.size() - n_samples_take + i];
      }

      memcpy(pcmf32.data() + n_samples_take, pcmf32_new.data(),
             n_samples_new * sizeof(float));

      pcmf32_old = pcmf32;

      whisper_full_params wparams = this->default_whisper_params();
      if (whisper_full(ctx, wparams, pcmf32.data(), pcmf32.size()) != 0) {
        response.set_message("reconize failed.");
        return Status::CANCELLED;
      }

      // step3: collect result
      const int n_segments = whisper_full_n_segments(ctx);
      std::stringstream ss;
      for (int i = 0; i < n_segments; ++i) {
        const char *text = whisper_full_get_segment_text(ctx, i);
        ss << text;
      }
      std::string iter_result = ss.str();

      for (const auto &r : whisper_result) {
        response.add_result(r);
      }
      ++n_iter;
      if (n_iter % n_new_line == 0) {
        // keep part of the audio for next iteration to try to mitigate word
        // boundary issues
        pcmf32_old =
            std::vector<float>(pcmf32.end() - n_samples_keep, pcmf32.end());
        whisper_result.push_back(iter_result);
      }

      // step4: get response
      response.add_result(iter_result);
      response.set_message("success");
      auto vad = response.mutable_vad_result();
      vad->set_is_talking(vad_simple(pcmf32_new, WHISPER_SAMPLE_RATE, 250, 0.6f,
                                     100.0f, false));
      stream->Write(response);
    }
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

  // struct whisper_context *ctx =
  // whisper_init_from_file(params.model.c_str());

  // if (ctx == nullptr) {
  //   fprintf(stderr, "error: failed to initialize whisper context\n");
  //   exit(3);
  // }

  std::string server_address = "0.0.0.0:" + params.port;
  WhisperStreamingServer service(params.model);

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
