#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import subprocess
import os

# 所有需要生成语音的文本列表（必须与 TTS.txt 完全一致）
TEXTS = [
    "你好，欢迎您的到来！有什么需要帮助的吗？",      # 1
    "好的，请跟我来。",                                   # 2
    "北京馆回望岁月变迁，见证时代发展，呈现北京日新月异的城市图景。",  # 3
    "这里就是北京馆啦，我要继续回去工作啦！",              # 4
    "广州馆整理馆藏史料，解读商都变迁，探寻广州千年岁月发展历史脉络。",  # 5
    "这里就是广州馆啦，我要继续回去工作啦！",              # 6
    "吉林馆整理馆藏文物，解读地域历史，探寻黑土地千年岁月发展历史脉络。",  # 7
    "这里就是吉林馆啦，我要继续回去工作啦！",              # 8
    "深圳馆规划滨海步道，徒步欣赏海景，沿途感受山海相依秀美城市风光。",  # 9
    "这里就是深圳馆啦，我要继续回去工作啦！",              # 10
    "上海馆展现江南风韵，融合现代潮流，塑造独树一帜都市文化气质。",  # 11
    "这里就是上海馆啦，我要继续回去工作啦！",              # 12
    "你好，管理员翁佳亮",                                  # 13
    "开始执行巡检任务",                                    # 14
    "北京馆未放置灭火器。",                                # 15
    "在北京馆发现火源。",                                  # 16
    "广州馆未放置灭火器。",                                # 17
    "在广州馆发现火源。",                                  # 18
    "吉林馆未放置灭火器。",                                # 19
    "在吉林馆发现火源。",                                  # 20
    "深圳馆未放置灭火器。",                                # 21
    "在深圳馆发现火源。",                                  # 22
    "上海馆未放置灭火器。",                                # 23
    "在上海馆发现火源。",                                  # 24
]

# ==========================================
# 语音参数配置（可调整）
# ==========================================
VOICE = "zh-CN-XiaoxiaoNeural"  # 语音角色
RATE = "+40%"                    # 语速：-50% 到 +50%，默认 +0%

def generate_tts():
    output_dir = "/home/reicom2025/ros_workspace/src/TTS"
    os.makedirs(output_dir, exist_ok=True)
    
    # 生成 TTS.txt
    tts_file = os.path.join(output_dir, "TTS.txt")
    with open(tts_file, 'w', encoding='utf-8') as f:
        for i, text in enumerate(TEXTS, 1):
            f.write(f"{i}. {text}\n")
    
    print(f"✅ TTS.txt 已生成，共 {len(TEXTS)} 条")
    print("=" * 60)
    
    # 生成 MP3
    success_count = 0
    skip_count = 0
    fail_count = 0
    
    for i, text in enumerate(TEXTS, 1):
        mp3_file = os.path.join(output_dir, f"{i}.mp3")
        
        # 如果文件已存在，跳过
        if os.path.exists(mp3_file):
            print(f"⏭️  [{i}/24] {i}.mp3 已存在，跳过")
            skip_count += 1
            continue
        
        print(f"🔄 [{i}/24] 生成 {i}.mp3: {text[:30]}{'...' if len(text) > 30 else ''}")
        
        try:
            cmd = [
                "edge-tts",
                "--voice", VOICE,
                "--rate", RATE,      # 添加语速控制
                "--text", text,
                "--write-media", mp3_file
            ]
            result = subprocess.run(cmd, check=True, capture_output=True, text=True)
            print(f"✅ 成功生成 {i}.mp3")
            success_count += 1
        except subprocess.CalledProcessError as e:
            print(f"❌ 生成 {i}.mp3 失败: {e.stderr if e.stderr else '未知错误'}")
            fail_count += 1
        except FileNotFoundError:
            print("❌ 找不到 edge-tts 命令，请先安装: pip install edge-tts")
            return
        except Exception as e:
            print(f"❌ 生成 {i}.mp3 时发生异常: {str(e)}")
            fail_count += 1
    
    # 打印统计信息
    print("=" * 60)
    print(f"📊 生成完成！成功: {success_count}, 跳过: {skip_count}, 失败: {fail_count}")
    print("=" * 60)

if __name__ == "__main__":
    generate_tts()