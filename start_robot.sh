#!/bin/bash

# 1. 启动主程序
gnome-terminal --tab --title="Main" -- bash -c "roslaunch reicoures_main start_all.launch; exec bash"

# 适当延时，等待主节点初始化完成
sleep 3 

# 2. 启动人脸检测
gnome-terminal --tab --title="Face Detect" -- bash -c "roslaunch face_rec face_detection.launch; exec bash"

sleep 2

# 3. 启动人脸识别验证
gnome-terminal --tab --title="Face Verify" -- bash -c "roslaunch face_rec face_verification.launch; exec bash"

sleep 2

# 4. 执行清理代价地图脚本 
gnome-terminal --tab --title="Clear Costmap" -- bash -c "python3 clear_costmap.py; exec bash"

sleep 1
# 5. 开启相对移动服务
gnome-terminal --tab --title="Rel Move" -- bash -c "roslaunch relative_move relative_move.launch; exec bash"

sleep 1
# 6. 开启二次定位服务
gnome-terminal --tab --title="AR Pose" -- bash -c "roslaunch ar_pose ar_base_sim.launch; exec bash"

sleep 1
# 7. 启动 bobac3 仿真环境(开启障碍物)
gnome-terminal --tab --title="Simulator" -- bash -c "cd /home/reicom2025/zhangaiwu && ./reinovo_bobac3_sim; exec bash"

sleep 1
#启动重定位自动充电节点
gnome-terminal --tab --title="Auto Charge" -- bash -c "rosrun reicoures_relocalization auto_charge_node; exec bash"

sleep 1
gnome-terminal --tab --title="Yolo_detector" -- bash -c "cd src/yolo_vision/scripts && uv run yolo_visualizer.py; exec bash"