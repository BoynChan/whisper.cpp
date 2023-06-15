#include "whisper.h"
#include <iostream>
#include <vector>

// command-line parameters
struct whisper_streaming_params {
  std::string model = "models/ggml-base.en.bin";
  std::string port = "50010";
};

void whisper_streaming_print_usage(int /*argc*/, char **argv,
                                   const whisper_streaming_params &params) {
  fprintf(
      stderr,
      "Check the fucking READMD: "
      "https://github.com/BoynChan/whisper.cpp/blob/master/README_myshell.md");
}

bool whisper_streaming_params_parse(int argc, char **argv,
                                    whisper_streaming_params &params) {
  for (int i = 1; i < argc; i++) {
    std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      whisper_streaming_print_usage(argc, argv, params);
      exit(0);
    } else if (arg == "-p" || arg == "--port") {
      params.port = argv[++i];
    } else if (arg == "-m" || arg == "--model") {
      params.model = argv[++i];
    } else {
      fprintf(stderr, "error: unknown argument: %s\n", arg.c_str());
      whisper_streaming_print_usage(argc, argv, params);
      exit(0);
    }
  }

  return true;
}

class Session {
public:
  Session() {
    std::vector<float> pcmf32(n_samples_30s, 0.0f);
    std::vector<float> pcmf32_old;
    n_iter = 0;
    ctx = nullptr;
    n_newline = std::max(1, length_ms / step_ms -
                                1); // number of steps to print new line
  }
  ~Session() {
    if (ctx != nullptr) {
      whisper_free(ctx);
    }
  }

  const int step_ms = 500;
  const int length_ms = 10000; // a sentence will have 10s
  const int keep_ms = 500;     // keep 200ms audio
  const int n_samples_step = (1e-3 * step_ms) * WHISPER_SAMPLE_RATE;
  const int n_samples_len = (1e-3 * length_ms) * WHISPER_SAMPLE_RATE;
  const int n_samples_keep = (1e-3 * keep_ms) * WHISPER_SAMPLE_RATE;
  const int n_samples_30s = (1e-3 * 30000.0) * WHISPER_SAMPLE_RATE;
  struct whisper_context *ctx;
  std::vector<float> pcmf32;
  std::vector<float> pcmf32_old;
  std::vector<std::string> whisper_result;
  int n_iter;
  int n_newline;
};