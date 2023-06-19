#include <stdint.h>
#include <websocketpp/config/asio_no_tls.hpp>

#include "common.h"
#include "json.hpp"
#include "streaming_common.h"
#include "webrtc/webrtc.hpp"
#include "websocketpp/common/connection_hdl.hpp"
#include "whisper.h"
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <websocketpp/server.hpp>
using namespace webrtc;
using namespace std::chrono;

using json = nlohmann::json;

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::connection_hdl;

using websocketpp::lib::bind;
using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

// pull out the type of messages sent by our config
typedef server::message_ptr message_ptr;

class streaming_server {
public:
  streaming_server(std::string model_path) {
    m_server.init_asio();

    m_server.set_open_handler(bind(&streaming_server::on_open, this, ::_1));
    m_server.set_close_handler(bind(&streaming_server::on_close, this, ::_1));
    m_server.set_message_handler(
        bind(&streaming_server::on_message, this, ::_1, ::_2));
    m_server.set_access_channels(websocketpp::log::elevel::none);
    m_server.clear_access_channels(websocketpp::log::alevel::none);

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
    fprintf(stderr, "system_info: n_threads = %d / %d | %s\n",
            std::min(8, (int32_t)std::thread::hardware_concurrency()) * 1,
            std::thread::hardware_concurrency(), whisper_print_system_info());
  }

  ~streaming_server() {
    m_server.stop_listening();
    m_server.stop();
  }

  whisper_full_params default_whisper_params() {

    whisper_full_params wparams =
        whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

    wparams.print_progress = false;
    wparams.print_special = false;
    wparams.print_realtime = false;
    wparams.print_timestamps = false;
    wparams.translate = false;
    wparams.detect_language = false;
    // wparams.max_tokens = 32;
    wparams.language = "en";
    wparams.n_threads =
        std::min(8, (int32_t)std::thread::hardware_concurrency());
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

  bool vad_webrtc(connection_hdl hdl, const std::vector<float> pcm) {
    auto s = this->m_connections[hdl];
    const int frameSize = 320; // 每个子数组的长度
    int true_cnt = 0;
    int false_cnt = 0;

    std::vector<int16_t> result(frameSize);

    int numFrames = std::ceil(static_cast<float>(pcm.size()) /
                              frameSize); // 计算分成多少个子数组

    for (int f = 0; f < numFrames; ++f) {
      // 分别将每个子数组转换为对应的int16_t数组并进行检测
      for (int i = 0; i < frameSize && f * frameSize + i < pcm.size(); ++i) {
        result[i] = static_cast<int16_t>(pcm[f * frameSize + i] * 32768.0);
      }

      auto ret = s->vad.IsSpeech(result.data(), frameSize, WHISPER_SAMPLE_RATE);
      // std::cout << "vad result: " << ret << std::endl;
      if (ret > 0) {
        true_cnt++;
      } else {
        false_cnt++;
      }
    }

    // 如果true_cnt比false_cnt大，则返回true，否则返回false
    return true_cnt > false_cnt;
  }

  void on_open(connection_hdl hdl) {
    std::shared_ptr<Session> s = std::make_shared<Session>();
    s->ctx = whisper_init_from_buffer(this->model_buffer.data(),
                                      this->model_buffer.size());
    // s->vad = Vad(Vad::kVadAggressive);
    this->m_connections[hdl] = s;
  }

  void on_close(connection_hdl hdl) { m_connections.erase(hdl); }

  void on_message(connection_hdl hdl, server::message_ptr msg) {
    auto s = this->m_connections[hdl];
    // check for a special command to instruct the server to stop listening so
    // it can be cleanly exited.
    if (msg->get_opcode() == 1) {
      // TEXT
    }
    if (msg->get_opcode() == 2) {
      // BINARY
    }
    // Convert the binary payload to a float32 array
    std::vector<int16_t> pcmi16_new(msg->get_payload().size() /
                                    sizeof(int16_t));
    std::memcpy(pcmi16_new.data(), msg->get_payload().data(),
                msg->get_payload().size());

    std::vector<float> pcmf32_new(pcmi16_new.size());
    for (int i = 0; i < pcmi16_new.size(); i++) {
      pcmf32_new[i] = float(pcmi16_new[i]) / float(32768.0);
    }

    // Get the size of the array and prepare the response
    const int n_samples_new = pcmf32_new.size();
    const int n_samples_take = std::min(
        (int)s->pcmf32_old.size(),
        std::max(0, s->n_samples_keep + s->n_samples_len - n_samples_new));
    s->pcmf32.resize(n_samples_new + n_samples_take);

    for (int i = 0; i < n_samples_take; i++) {
      s->pcmf32[i] = s->pcmf32_old[s->pcmf32_old.size() - n_samples_take + i];
    }

    memcpy(s->pcmf32.data() + n_samples_take, pcmf32_new.data(),
           n_samples_new * sizeof(float));

    s->pcmf32_old = s->pcmf32;

    whisper_full_params wparams = this->default_whisper_params();
    if (whisper_full_parallel(s->ctx, wparams, s->pcmf32.data(),
                              s->pcmf32.size(), 1) != 0) {
      this->m_server.send(hdl, "error", websocketpp::frame::opcode::text);
    }

    // step3: collect result
    const int n_segments = whisper_full_n_segments(s->ctx);
    std::stringstream ss;
    for (int i = 0; i < n_segments; ++i) {
      const char *text = whisper_full_get_segment_text(s->ctx, i);
      ss << text;
    }
    std::string iter_result = ss.str();

    // for (const auto &r : whisper_result) {
    //   response.add_result(r);
    // }
    std::vector<std::string> result_response(s->whisper_result.begin(),
                                             s->whisper_result.end());
    result_response.push_back(iter_result);
    ++s->n_iter;
    if (s->n_iter % s->n_newline == 0) {
      // keep part of the audio for next iteration to try to mitigate word
      // boundary issues
      s->pcmf32_old = std::vector<float>(s->pcmf32.end() - s->n_samples_keep,
                                         s->pcmf32.end());
      s->whisper_result.push_back(iter_result);
    }

    json j;
    j["result"] = result_response;
    j["is_talking"] = this->vad_webrtc(hdl, pcmf32_new);
    try {
      this->m_server.send(hdl, j.dump(), websocketpp::frame::opcode::text);
    } catch (websocketpp::exception const &e) {
      std::cout << "Echo failed because: "
                << "(" << e.what() << ")" << std::endl;
    }
  }

  void run(uint16_t port) {
    std::cout << "Listening on port " << port << std::endl;
    m_server.set_reuse_addr(true);
    m_server.listen(port);
    m_server.start_accept();
    m_server.run();
  }

private:
  typedef std::map<connection_hdl, std::shared_ptr<Session>,
                   std::owner_less<connection_hdl>>
      con_list;
  std::vector<char> model_buffer;
  server m_server;
  con_list m_connections;
};

int main(int argc, char **argv) {
  whisper_streaming_params params;

  if (!whisper_streaming_params_parse(argc, argv, params)) {
    return 1;
  }
  // Create a server endpoint
  streaming_server server(params.model);

  try {
    server.run(std::stoi(params.port));
  } catch (websocketpp::exception const &e) {
    std::cout << e.what() << std::endl;
  } catch (...) {
    std::cout << "other exception" << std::endl;
  }
}
