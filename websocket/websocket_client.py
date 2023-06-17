import asyncio
import websockets
import sys
import wavio
import numpy as np
import time

def read_wav_file(filename):
    wav = wavio.read(filename)
    pcmf32 = wav.data[:, 0].astype(np.float32) / 32768.0
    return pcmf32


def divide_chunks(l, n=8000):
    for i in range(0, len(l), n):
        yield l[i:i + n]

async def echo_websocket_client(uri):
    numerical_samples = read_wav_file("../samples/mm0.wav")
    audio_chunks = list(divide_chunks(numerical_samples, 8000))
    async with websockets.connect(uri) as websocket:
        for idx, chunk in enumerate(audio_chunks):
            # Convert chunk to binary data and send over websocket
            binary_data = chunk.tobytes()
            start_time = time.time()
            await websocket.send(binary_data)

            # Receive response from websocket server
            response = await websocket.recv()
            print(f"已收到第 {idx + 1} 个数据块的回复: {response}")
            # 记录结束时间
            end_time = time.time()

            elapsed_time = end_time - start_time

            print(f"函数执行耗时: {elapsed_time:.6f}秒")


uri = "ws://localhost:9002"
asyncio.get_event_loop().run_until_complete(echo_websocket_client(uri))