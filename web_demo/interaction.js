const url = "ws://34.133.234.87:5052";
var output = document.getElementById("output");
var start = document.getElementById("start");
var stop = document.getElementById("stop");

const aiRandom = [
    "Hi, how do you do",
    "As an AI Language Model, I don't know what you are talking about",
    "Nice to meet you, do you have anything to say",
    "I am a robot, I am not a human",
    "Do you know myshell? It's the coolest AI robot product."
]

const aiRandomURL = [
    "https://demo.boynn.xyz/test00.mp3",
    "https://demo.boynn.xyz/test01.mp3",
    "https://demo.boynn.xyz/test02.mp3",
    "https://demo.boynn.xyz/test03.mp3",
    "https://demo.boynn.xyz/test04.mp3",
]

const context = {
    webSocket: undefined,
    mediaStream: undefined,
    recorder: undefined,
};

const speaking_status = {
    start_talking: false,
    vad_false_count: 0,
    lask_vad_result: [],
    chat_history: [],
}

async function connect() {
    const socket = new WebSocket(url);
    context.webSocket = socket;

    socket.onopen = () => {
        console.info("socket connected");
    };
    socket.onmessage = async (event) => {
        const respPack = JSON.parse(event.data);
        output.value = `vad result:${respPack.is_talking
            }\n\n${respPack.result.join("\n")}`;
        speaking_status.lask_vad_result = respPack.result;

        if (!respPack.is_talking && speaking_status.start_talking) {
            speaking_status.vad_false_count++;
        } else {
            speaking_status.vad_false_count = 0;
            speaking_status.start_talking = true;
        }
        if (speaking_status.vad_false_count > 3) {

            onStop();
        }

    };
}

async function onStart() {
    start.disabled = true;
    stop.disabled = false;

    connect();
    const socket = context.webSocket;
    context.mediaStream = await window.navigator.mediaDevices.getUserMedia({
        audio: true,
    });
    context.recorder = new RecordRTC(context.mediaStream, {
        // 后端要求单通道，16K采样率，PCM
        type: "audio",
        recorderType: StereoAudioRecorder,
        mimeType: "audio/wav",
        numberOfAudioChannels: 1,
        desiredSampRate: 16000,
        disableLogs: true,
        timeSlice: 500,
        ondataavailable(recordResult) {
            if (!socket) {
                return;
            }

            // handleRecordResult?.(recordResult);

            const pcm = recordResult.slice(44);
            // console.log(pcm);
            // const data = encodeAudioOnlyRequest(pcm);
            if (socket.readyState === socket.OPEN) {
                socket.send(pcm);
            }
        },
    });

    context.recorder.startRecording();
}

function downloadBlob(blob, fileName) {
    // 创建一个a标签并附加属性
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = fileName;
    a.style.display = "none";

    // 将a标签添加到文档，触发下载后移除
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);

    // 释放创建的URL对象
    URL.revokeObjectURL(a.href);
}

function appendHistoryAndRender(chat_record) {
    speaking_status.chat_history.push({ role: chat_record.role, content: chat_record.content })
    document.getElementById("history").innerHTML = speaking_status.chat_history.map((item) => {
        return `<div class="chat_item ${item.role}">${item.role}: ${item.content}</div>`
    }).join("<br/>")
}

function talkToBot(content, history) {
    // set an time interval to execute this function
    setTimeout(() => {
        const responseIndex = Math.floor(Math.random() * aiRandom.length)
        appendHistoryAndRender({ role: "bot", content: aiRandom[responseIndex] })
        const audioElement = document.createElement('audio');

        // 添加“ended”事件监听器
        audioElement.addEventListener('ended', () => {
            onStart()
        });

        // 设置音频播放源
        audioElement.src = aiRandomURL[responseIndex];

        // 将音频元素添加到页面
        document.body.appendChild(audioElement);

        // 播放音频
        audioElement.play();

        // 一个示例函数，表示要在音频播放完成后执行的下一步操作
    }, 1000)
}

async function playTTS() {
    const buf = await tts(aiRandom[0])
    console.log(`buf length:${buf.size}`)
}

function onFinish() {
    start.disabled = false;
    stop.disabled = true;

    if (!context.recorder) {
        return;
    }
    context.recorder.stopRecording(() => {
        if (!context.webSocket) {
            return;
        }
        context.webSocket.close();

        context.mediaStream?.getAudioTracks().forEach((item) => item.stop());
    });
}

function onStop() {
    speaking_status.vad_false_count = 0;
    speaking_status.start_talking = false;

    appendHistoryAndRender({ role: "human", content: speaking_status.lask_vad_result.join("") })
    talkToBot(speaking_status.lask_vad_result.join(""), speaking_status.chat_history)

    onFinish()
}