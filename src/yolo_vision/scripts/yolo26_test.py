from ultralytics import YOLO
import cv2

# 1. 加载 YOLO26 模型
model = YOLO("yolo26n.pt")

# 2. 打开 /dev/video0 视频流
# 在 Linux 中，系统会自动将 0 映射为 /dev/video0
cap = cv2.VideoCapture(0)

if not cap.isOpened():
    print("错误：无法打开 /dev/video0，请检查摄像头是否成功挂载到虚拟机。")
    exit()

print("成功打开 /dev/video0，正在启动 YOLO26 推理... 按 'q' 键退出。")

while cap.isOpened():
    success, frame = cap.read()
    if not success:
        print("未能获取到视频帧。")
        break

    # 3. 对当前帧进行 YOLO26 推理
    # stream=True 可以使用流式生成器，大大降低内存占用
    results = model(frame, stream=True)

    for r in results:
        # 获取带有边界框和标签的渲染画面
        annotated_frame = r.plot()
        
        # 4. 在窗口中实时显示
        cv2.imshow("YOLO26 Live Stream (/dev/video0)", annotated_frame)

    # 按 'q' 键退出循环
    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

# 释放资源
cap.release()
cv2.destroyAllWindows()