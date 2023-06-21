from faster_whisper import WhisperModel
import time
import wavio
import numpy as np

model_size = "base"


def read_wav_file(filename):
    wav = wavio.read(filename)
    pcmf32 = wav.data[:, 0].astype(np.float32) / 32768.0
    return pcmf32


def divide_chunks(l, n=16000):
    for i in range(0, len(l), n):
        yield l[i:i + n]


def main():

    # Run on GPU with FP16
    # model = WhisperModel(model_size, device="cuda", compute_type="float16")

    # or run on GPU with INT8
    # model = WhisperModel(model_size, device="cuda", compute_type="int8_float16")
    # or run on CPU with INT8
    start_time = time.time()
    model = WhisperModel(model_size, device="cpu",
                         compute_type="int8", download_root="./models")

    end_time = time.time()
    elapsed_time = end_time - start_time
    print(f"model load cost: {elapsed_time:.6f}秒")

    numerical_samples = read_wav_file("../samples/mm0.wav")
    audio_chunks = list(divide_chunks(numerical_samples, 8000))

    for idx, chunk in enumerate(audio_chunks):
        start_time = time.time()
        segments, info = model.transcribe(chunk, beam_size=5, language="en")

        segment = " ".join(map(lambda x: x.text, list(segments)))
        print(segment)
        end_time = time.time()
        elapsed_time = end_time - start_time
        print(f"transcribe cost: {elapsed_time:.6f}秒")


if __name__ == '__main__':
    main()
