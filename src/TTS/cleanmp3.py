#!/usr/bin/env python3
import os
import glob

tts_dir = "/home/reicom2025/ros_workspace/src/TTS"
mp3_files = glob.glob(os.path.join(tts_dir, "*.mp3"))

if mp3_files:
    print(f"找到 {len(mp3_files)} 个 mp3 文件：")
    for f in mp3_files:
        print(f"  - {os.path.basename(f)}")
    
    confirm = input("确认删除所有 mp3 文件？(y/n): ")
    if confirm.lower() == 'y':
        for f in mp3_files:
            os.remove(f)
            print(f"已删除: {os.path.basename(f)}")
        print("删除完成！")
else:
    print("没有找到 mp3 文件")