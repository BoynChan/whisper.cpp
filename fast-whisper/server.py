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
from faster_whisper.vad import (
    VadOptions,
    get_speech_timestamps,
)


class WebSocketServer:
    def __init__(self):
        self.sentence_split_duration = 10

    async def handler(self, websocket, path):
        model_size = "base"
        model = WhisperModel(model_size, device="cuda",
                             compute_type="int8", download_root="./models")
        pcmf32: np.ndarray[np.float32, Any] = np.empty(
            (0), dtype=np.float32)
        pcmf32_old: np.ndarray[np.float32, Any] = np.empty(
            (0), dtype=np.float32)
        whisper_result: List(str) = []
        last_response_result: List(str) = []

        vad_parameters = VadOptions()

        try:
            async for message in websocket:
                if len(message) == 0:
                    break
                try:
                    # 尝试解析消息为JSON
                    msg = json.loads(message)
                    if msg["command"] == "clear":
                        pcmf32_old = np.empty((0), dtype=np.float32)
                        pcmf32 = np.empty((0), dtype=np.float32)
                        whisper_result = []
                        last_response_result = []
                    continue
                except Exception:
                    pass

                pcmf32_new = np.array(struct.unpack(
                    f'{len(message) // 2}h', message), dtype=np.int16).astype(np.float32) / 32768.0
                pcmf32 = np.concatenate((pcmf32_old, pcmf32_new))
                segments, info = model.transcribe(
                    pcmf32, beam_size=5, language="en", without_timestamps=True)
                pcmf32_old = pcmf32
                new_speech_chunks = get_speech_timestamps(
                    pcmf32_new, vad_parameters)
                all_speech_chunks = get_speech_timestamps(
                    pcmf32, vad_parameters)
                segment = " ".join(map(lambda x: x.text, list(segments)))
                # 如果VAD检测不到在说话, 并且一整句话中也没有在说话, 则将输出的结果设置为空
                if new_speech_chunks.__len__() == 0 and all_speech_chunks.__len__() == 0:
                    segment = ""

                response_result = whisper_result[:]
                response_result.append(segment.strip())
                if info.duration >= 10:
                    whisper_result.append(segment.strip())
                    # 保留100ms的音频
                    pcmf32_old = pcmf32[-100 * 16:].copy()

                # 记录上一次结果, 如果VAD判断为真但是结果没有任何变化, 则记为不在说话
                is_talking = new_speech_chunks.__len__() > 0
                if is_talking and response_result == last_response_result:
                    is_talking = False
                last_response_result = response_result

                response = {"result": response_result,
                            "duration": info.duration, "is_talking": is_talking, "speech_chunks": new_speech_chunks}
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
