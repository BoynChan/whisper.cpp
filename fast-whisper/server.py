import asyncio
import websockets
from websockets.exceptions import ConnectionClosedError
import json
from faster_whisper import WhisperModel
import time
import wavio
import numpy as np
import struct
from typing import List, Any


class WebSocketServer:
    def __init__(self):
        self.sentence_split_duration = 10

    async def handler(self, websocket, path):
        model_size = "base"
        model = WhisperModel(model_size, device="cpu",
                                  compute_type="int8", download_root="./models")
        pcmf32: np.ndarray[np.float32, Any] = np.empty(
            (0), dtype=np.float32)
        pcmf32_old: np.ndarray[np.float32, Any] = np.empty(
            (0), dtype=np.float32)
        whisper_result: List(str) = []

        try:
            async for message in websocket:
                if len(message) == 0:
                    break
                pcmf32_new = np.array(struct.unpack(
                    f'{len(message) // 2}h', message), dtype=np.int16).astype(np.float32) / 32768.0
                pcmf32 = np.concatenate((pcmf32_old, pcmf32_new))
                segments, info = model.transcribe(
                    pcmf32, beam_size=5, language="en", without_timestamps=True)
                pcmf32_old = pcmf32
                segment = " ".join(map(lambda x: x.text, list(segments)))

                response_result = whisper_result[:]
                response_result.append(segment.strip())
                if info.duration >= 10:
                    whisper_result.append(segment.strip())
                    # 保留100ms的音频
                    pcmf32_old = pcmf32[-100 * 16:].copy()

                response = {"response": response_result,
                            "duration": info.duration}
                await websocket.send(json.dumps(response))
        except ConnectionClosedError:
            print("peer closed")

    async def start_server(self, host='localhost', port=5003):
        async with websockets.serve(self.handler, host, port):
            print(f"WebSocket服务启动成功，正在监听 {host}:{port}")
            await asyncio.Future()


if __name__ == "__main__":
    server = WebSocketServer()

    try:
        asyncio.run(server.start_server())
    except KeyboardInterrupt:
        print("WebSocket服务器已关闭")
