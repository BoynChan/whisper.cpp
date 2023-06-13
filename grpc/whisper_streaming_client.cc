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
#include <string>

#include <grpcpp/grpcpp.h>

#include "common.h"
#include "whisper.h"
#include "whisper_streaming.grpc.pb.h"

using namespace myshell;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

class WhisperStreamingClient {
public:
  WhisperStreamingClient(std::shared_ptr<Channel> channel)
      : stub_(WhisperStreaming::NewStub(channel)) {}

  // Assembles the client's payload, sends it and presents the response back
  // from the server.
  std::string Ping() {
    // Data we are sending to the server.
    EmptyReq request;

    // Container for the data we expect from the server.
    PingReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->Ping(&context, request, &reply);

    // Act upon its status.
    if (status.ok()) {
      return reply.message();
    } else {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      return "RPC failed";
    }
  }

  std::string Transcribe(std::string file_name) {
    std::vector<float> pcmf32;               // mono-channel F32 PCM
    std::vector<std::vector<float>> pcmf32s; // stereo-channel F32 PCM
    if (!::read_wav(file_name, pcmf32, pcmf32s, false)) {
      fprintf(stderr, "error: failed to read WAV file '%s'\n",
              file_name.c_str());
      return "";
    }
    TranscribeRequest req;
    google::protobuf::RepeatedField<float> *values = req.mutable_audio_data();
    for (float v : pcmf32) {
      values->Add(v);
    }
    TranscribeResponse res;
    ClientContext context;

    std::cout << "ASR audio length:" << pcmf32.size() << std::endl;
    Status status = stub_->Transcribe(&context, req, &res);

    if (status.ok()) {
      return res.text();
    } else {
      std::cout << "MyMethod RPC failed - error: " << status.error_message()
                << " code: " << status.error_code() << std::endl;
      return "RPC failed";
    }
  }

  void TranscribeStreamingByFile(std::string file_name) {
    std::vector<float> pcmf32;               // mono-channel F32 PCM
    std::vector<std::vector<float>> pcmf32s; // stereo-channel F32 PCM
    if (!::read_wav(file_name, pcmf32, pcmf32s, false)) {
      fprintf(stderr, "error: failed to read WAV file '%s'\n",
              file_name.c_str());
      return;
    }

    std::cout << "ASR audio length:" << pcmf32.size() << std::endl;
    int cnt = 0;
    int default_step_ms = 500;
    int chunkSize = WHISPER_SAMPLE_RATE * default_step_ms / 1000;

    ClientContext context;
    auto stream = stub_->TranscribeStreaming(&context);
    while (cnt < pcmf32.size()) {
      StreamingTranscribeRequest req;
      const auto t_start = std::chrono::high_resolution_clock::now();
      for (int i = 0; i < chunkSize && cnt < pcmf32.size(); i++) {
        req.add_audio_data(pcmf32[cnt++]);
      }
      stream->Write(req);

      StreamingTranscibeReply reply;
      stream->Read(&reply);
      const auto t_now = std::chrono::high_resolution_clock::now();
      const auto t_end =
          std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_start)
              .count();

      std::cout << "get response:" << reply.result()[0] << std::endl;
      std::cout << "get vad_result:" << reply.vad_result().is_talking()
                << std::endl;
      std::cout << "used time: " << t_end << "ms"
                << "\n"
                << std::endl;
    }
    std::cout << "cnt:" << cnt << " total_size:" << pcmf32.size() << std::endl;
    stream->WritesDone();
    Status status = stream->Finish();
    if (!status.ok()) {
      std::cout << status.error_code() << ": " << status.error_message()
                << std::endl;
      std::cout << "RPC failed";
    }
  }

private:
  std::unique_ptr<WhisperStreaming::Stub> stub_;
};

int main(int argc, char **argv) {
  // Instantiate the client. It requires a channel, out of which the actual
  // RPCs are created. This channel models a connection to an endpoint
  // specified by the argument "--target=" which is the only expected
  // argument. We indicate that the channel isn't authenticated (use of
  // InsecureChannelCredentials()).
  WhisperStreamingClient client(
      grpc::CreateChannel("0.0.0.0:50010", grpc::InsecureChannelCredentials()));
  //   std::string reply = client.Ping();
  //   std::cout << "Greeter received: " << reply << std::endl;

  // auto result = client.Transcribe("../../samples/gb0.wav");
  // std::cout << "ASR Result:" << result << std::endl;
  client.TranscribeStreamingByFile("../../samples/gb0.wav");
  return 0;
}
