#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import rospy
import numpy as np
import cv2
from sensor_msgs.msg import Image
from std_msgs.msg import Bool  # 引入标准布尔消息
from ultralytics import YOLO

# ==========================================
# ⚙️ 核心配置区
# ==========================================
MODEL_PATH = "/home/reicom2025/ros_workspace/src/yolo_vision/models/best.pt"

class YoloDetectorNode:
    def __init__(self):
        rospy.init_node("yolo_detector", anonymous=True)
        
        rospy.loginfo("⏳ 正在加载 YOLO (Bool 状态机模式)...")
        self.model = YOLO(MODEL_PATH)
        rospy.loginfo("✅ 视觉神经已连通！开始实时同步 Bool 标志位。")

        # 🎯 创建两个独立的布尔发布者，完美对接 C++ 变量
        self.pub_fire = rospy.Publisher("/vision/fire_detected", Bool, queue_size=1)
        self.pub_ext = rospy.Publisher("/vision/extinguisher_detected", Bool, queue_size=1)
        
        self.sub_image = rospy.Subscriber("/berxel_base/color/image_raw", Image, self.image_callback)

    def image_callback(self, msg):
        try:
            # 1. 绕过 cv_bridge 的零拷贝光速解包
            if msg.encoding == "bgr8":
                frame = np.frombuffer(msg.data, dtype=np.uint8).reshape(msg.height, msg.width, 3)
            elif msg.encoding == "rgb8":
                frame = np.frombuffer(msg.data, dtype=np.uint8).reshape(msg.height, msg.width, 3)
                frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
            else:
                return

            # 2. 静默推理 (imgsz=320 极致压榨虚拟机 CPU 帧率)
            results = self.model(frame, imgsz=320, verbose=False)
            
            see_fire = False
            see_ext = False
            
            # 3. 提取结果
            for result in results:
                for box in result.boxes:
                    conf = float(box.conf[0])
                    if conf > 0.5: # 置信度阈值设为 50%
                        cls_id = int(box.cls[0])
                        label = self.model.names[cls_id] 
                        
                        if label == 'fire':
                            see_fire = True
                        elif label == 'fire_extinguisher':
                            see_ext = True
                            
            # 4. 每帧结束，无论 True 还是 False 都实时广播同步给 C++
            self.pub_fire.publish(Bool(data=see_fire))
            self.pub_ext.publish(Bool(data=see_ext))

        except Exception as e:
            rospy.logerr(f"❌ 视觉节点报错: {e}")

if __name__ == '__main__':
    try:
        detector = YoloDetectorNode()
        rospy.spin() 
    except rospy.ROSInterruptException:
        pass