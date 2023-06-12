// command-line parameters
struct whisper_streaming_params {
  std::string model = "models/ggml-base.en.bin";
  std::string port = "50010";
};

void whisper_streaming_print_usage(int /*argc*/, char ** argv, const whisper_streaming_params & params) {
    fprintf(stderr, "Check the fucking READMD: https://github.com/BoynChan/whisper.cpp/blob/master/README_myshell.md");
}

bool whisper_streaming_params_parse(int argc, char ** argv, whisper_streaming_params & params) {
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            whisper_streaming_print_usage(argc, argv, params);
            exit(0);
        }
        else if (arg == "-p"    || arg == "--port")           { params.port           = argv[++i]; }
        else if (arg == "-m"    || arg == "--model")          { params.model          = argv[++i]; }
        else {
            fprintf(stderr, "error: unknown argument: %s\n", arg.c_str());
            whisper_streaming_print_usage(argc, argv, params);
            exit(0);
        }
    }

    return true;
}
