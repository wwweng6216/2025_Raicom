#!/usr/bin/env python3
import json
import sys
import pyaudio
import os
import sys

from vosk import Model, KaldiRecognizer

# 设置模型路径（请改成你自己的实际路径）
current_script_dir = os.path.dirname(os.path.abspath(__file__))
# 基于脚本目录(scripts)，向上一级，进入 model，再进入真正的模型文件夹
MODEL_PATH = os.path.abspath(os.path.join(current_script_dir, "..", "..", "model", "vosk-model-small-cn-0.22"))

# 音频参数（必须与Vosk要求一致）
SAMPLE_RATE = 16000
CHUNK = 4096

def main():
    # 加载模型
    try:
        model = Model(MODEL_PATH)
    except Exception as e:
        print(f"❌ 模型加载失败，请检查路径: {MODEL_PATH}")
        print(f"错误信息: {e}")
        sys.exit(1)

    recognizer = KaldiRecognizer(model, SAMPLE_RATE)
    recognizer.SetWords(False)  # 不输出单词时间戳，只输出文本

    # 打开麦克风
    p = pyaudio.PyAudio()
    stream = p.open(
        format=pyaudio.paInt16,
        channels=1,
        rate=SAMPLE_RATE,
        input=True,
        frames_per_buffer=CHUNK
    )

    print("🎙️ 开始监听... (按 Ctrl+C 退出)")
    print("说中文吧，我会实时输出识别结果：")

    try:
        while True:
            data = stream.read(CHUNK, exception_on_overflow=False)
            if recognizer.AcceptWaveform(data):
                # 最终结果（静音检测到结束）
                result = json.loads(recognizer.Result())
                text = result.get("text", "")
                if text:
                    print(f"✅ {text}")
            else:
                # 可选：输出中间结果（实时草稿）
                partial = json.loads(recognizer.PartialResult())
                partial_text = partial.get("partial", "")
                if partial_text:
                    # 使用 \r 覆盖同一行，让草稿实时刷新
                    print(f"\r⏳ {partial_text}", end="", flush=True)
                else:
                    # 清空草稿行
                    print("\r" + " " * 50, end="", flush=True)
    except KeyboardInterrupt:
        print("\n👋 退出")
    finally:
        stream.stop_stream()
        stream.close()
        p.terminate()

if __name__ == "__main__":
    main()