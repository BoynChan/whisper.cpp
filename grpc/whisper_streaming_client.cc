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

#include "whisper_streaming.grpc.pb.h"

using namespace myshell;
using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

class WhisperStreamingClient
{
public:
    WhisperStreamingClient(std::shared_ptr<Channel> channel)
        : stub_(WhisperStreaming::NewStub(channel)) {}

    // Assembles the client's payload, sends it and presents the response back
    // from the server.
    std::string Ping()
    {
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
        if (status.ok())
        {
            return reply.message();
        }
        else
        {
            std::cout << status.error_code() << ": " << status.error_message()
                      << std::endl;
            return "RPC failed";
        }
    }

private:
    std::unique_ptr<WhisperStreaming::Stub> stub_;
};

int main(int argc, char **argv)
{
    // Instantiate the client. It requires a channel, out of which the actual RPCs
    // are created. This channel models a connection to an endpoint specified by
    // the argument "--target=" which is the only expected argument.
    // We indicate that the channel isn't authenticated (use of
    // InsecureChannelCredentials()).
    WhisperStreamingClient client(
        grpc::CreateChannel("0.0.0.0:50010", grpc::InsecureChannelCredentials()));
    std::string reply = client.Ping();
    std::cout << "Greeter received: " << reply << std::endl;

    return 0;
}
