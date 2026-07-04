#!/home/reicom2025/ros_workspace/.venv/bin/python3
# -*- coding: utf-8 -*-

import rospy
import numpy as np
import cv2
from sensor_msgs.msg import Image
from ultralytics import YOLO
from std_msgs.msg import String

# ==========================================
# ⚙️ 配置区（保持与检测节点一致）
# ==========================================
MODEL_PATH = "/home/reicom2025/ros_workspace/src/yolo_vision/models/best.pt"

class YoloVisualizerNode:
    def __init__(self):
        rospy.init_node("yolo_visualizer", anonymous=True)
        rospy.loginfo("⏳ 正在加载 YOLO 可视化调试模型...")
        
        # 加载模型用于本地渲染画框
        self.model = YOLO(MODEL_PATH)
        rospy.loginfo("✅ 调试窗口已准备就绪，正在等待相机画面...")

        # 订阅与检测节点完全相同的原始图像话题
        self.sub_image = rospy.Subscriber("/berxel_base/color/image_raw", Image, self.image_callback)

        # 【新增】创建发布者，将检测结果发给主控大脑
        self.yolo_pub = rospy.Publisher('/yolo_detection_result', String, queue_size=10)

    def image_callback(self, msg):
        try:
            # 1. 绕过 cv_bridge 快速解包
            if msg.encoding == "bgr8":
                frame = np.frombuffer(msg.data, dtype=np.uint8).reshape(msg.height, msg.width, 3).copy()
            elif msg.encoding == "rgb8":
                frame = np.frombuffer(msg.data, dtype=np.uint8).reshape(msg.height, msg.width, 3)
                frame = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
            else:
                return

            # 2. 推理并允许渲染 (注意：这里去掉了 verbose=False，方便看耗时)
            # 我们直接使用 YOLO 自带的 .plot() 功能来画框
            results = self.model(frame, imgsz=320)

            # ================= 新增：解析与发布逻辑 =================
            detected_classes = []
            if len(results[0].boxes) > 0:
                cls_ids = results[0].boxes.cls.int().tolist()
                detected_classes = [results[0].names[cls_id] for cls_id in cls_ids]
            
            # 核心原则：只发布“实际看到”的目标，不发布“没看到”的目标
            if 'fire' in detected_classes:
                self.yolo_pub.publish("fire")
                
            if 'extinguisher' in detected_classes:
                self.yolo_pub.publish("extinguisher")
            # =======================================================
            
            # 3. 自动绘制带有置信度和标签的边界框
            annotated_frame = results[0].plot()

            # 4. 加上调试小工具：实时显示帧率或提示
            cv2.putText(annotated_frame, "DEBUG WINDOW - PRESS 'Q' TO EXIT", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)

            # 5. 弹出 OpenCV 窗口供人类查看
            cv2.imshow("YOLO Real-time Detection Debug", annotated_frame)
            
            # 必须加 waitKey，否则窗口会卡死；监听 'q' 键退出
            if cv2.waitKey(1) & 0xFF == ord('q'):
                rospy.signal_shutdown("User requested exit.")

        except Exception as e:
            rospy.logerr(f"❌ 可视化节点报错: {e}")

    def clean_up(self):
        cv2.destroyAllWindows()

if __name__ == '__main__':
    try:
        visualizer = YoloVisualizerNode()
        rospy.spin()
    except rospy.ROSInterruptException:
        pass
    finally:
        cv2.destroyAllWindows()