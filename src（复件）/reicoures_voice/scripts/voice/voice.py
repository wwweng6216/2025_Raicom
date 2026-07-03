#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import rospy
from std_msgs.msg import String
import json
import os
import sys

# 尝试导入必要的库，给报错
try:
    import vosk
    import pyaudio
except ImportError:
    print("缺少依赖库！请在终端运行: pip3 install vosk pyaudio")
    sys.exit(1)

def main():
    # 1. 初始化 ROS 节点
    rospy.init_node('vosk_recognizer_node', anonymous=False)
    
    # 2. 创建一个发布者，往 /vosk_result 话题发字符串,队列长度为10
    pub = rospy.Publisher('/vosk_result', String, queue_size=10)
    
    # 3. 设置 Vosk 模型路径 (使用动态相对路径)
    current_script_dir = os.path.dirname(os.path.abspath(__file__))
    default_model_path = os.path.abspath(os.path.join(current_script_dir, "..", "..","model", "vosk-model-small-cn-0.22"))
    
    model_path = rospy.get_param("~model_path", default_model_path)
    
    if not os.path.exists(model_path):
        rospy.logerr(f"❌ 找不到 Vosk 模型目录：{model_path}")
        rospy.logerr("请修改代码里的模型路径，或将模型拷贝到该目录下！")
        return

    rospy.loginfo(f"正在加载 Vosk 模型: {model_path} ... (等几秒钟!)")
    model = vosk.Model(model_path)
    rec = vosk.KaldiRecognizer(model, 16000)
    
    # 4. 初始化 PyAudio 麦克风录音
    p = pyaudio.PyAudio()
    stream = p.open(format=pyaudio.paInt16, channels=1, rate=16000, input=True, frames_per_buffer=8000)
    stream.start_stream()
    
    rospy.loginfo("=======================================")
    rospy.loginfo("🎤 Vosk 语音节点已启动，请对着麦克风说话...")
    rospy.loginfo("=======================================")
    
    # 5. 死循环监听
    while not rospy.is_shutdown():
        try:
            # 读取麦克风数据
            data = stream.read(4000, exception_on_overflow=False)
            
            # 如果识别到了完整的一句话
            if rec.AcceptWaveform(data):
                result_json = rec.Result()
                result_dict = json.loads(result_json)
                text = result_dict.get("text", "")
                
                # Vosk 输出的中文通常带有空格（如 "深 圳 馆"），这里将其去掉
                # text = text.replace(" ", "")
                
                if text:
                    rospy.loginfo(f"✅ 识别成功: {text}")
                    
                    # 6. 将文字发布到 ROS 话题
                    msg = String()
                    msg.data = text
                    pub.publish(msg)
                    
        except Exception as e:
            rospy.logwarn(f"读取音频数据出错: {e}")
            
    # 程序关闭时清理资源
    rospy.loginfo("正在关闭音频流...")
    stream.stop_stream()
    stream.close()
    p.terminate()

if __name__ == '__main__':
    try:
        main()
    except rospy.ROSInterruptException:
        pass