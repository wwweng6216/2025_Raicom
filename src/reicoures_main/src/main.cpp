#include <ros/ros.h>
#include <std_msgs/String.h>
#include <std_srvs/SetBool.h>
#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <cctype>

// ==========================================
// 1. 状态机枚举定义 (涵盖任务一与任务二)
// ==========================================
enum RobotState {
    STATE_IDLE,                 // 待机状态：在出发区等待人脸检测
    STATE_T1_WAIT_VOICE,        // 任务一：已热烈欢迎，等待评委说出目的地
    STATE_T1_NAVIGATING,        // 任务一：开车导览前往目标馆中
    STATE_T1_ARRIVED_DES,       // 任务一：到达目标馆，播放介绍和结束语
    STATE_T2_WAIT_START,        // 任务二：认出管理员，等待说“开始执行巡检任务”
    STATE_T2_INSPECTING,        // 任务二：智能巡检遍历场馆中
    STATE_T2_CHARGING,          // 任务二：控制权移交给重定位包，正在自动充电中
    STATE_RETURN_HOME           // 终点连招：返回绝对初始点
};

// ==========================================
// 2. 官方标准台词文本智库 (一字不差)
// ==========================================
struct PavilionVoice {
    std::string intro;
    std::string no_extinguisher;
    std::string fire_found;
    std::string goodbye;
};

// ==========================================
// 3. 展馆字典结构体
// ==========================================
struct Destination {
    std::string cmd;  // 下发给老司机的英文代号
    std::string name; // 用于日志打印的中文名称
};

const std::string VOICE_GREETING    = "你好游客，欢迎您的到来！有什么需要帮助的吗？";
const std::string VOICE_GUIDE_START = "好的，请跟我来";
const std::string VOICE_INSPECT_MOD = "开始执行巡检任务";

std::map<std::string, PavilionVoice> pavilion_dict = {
    {"beijing",   {"北京，中国首都，千年古都与现代都市交融，尽显独特魅力。这里有宏伟的故宫、绵延的长城等历史古迹，见证着岁月的沧桑变迁。", "北京馆未放置灭火器。", "在北京馆发现火源。", "这里就是北京馆啦，我要继续回去工作啦！"}},
    {"guangzhou", {"广州，别称羊城、花城，广东省会。历史悠久，美食诱人，经济发达，是充满魅力与活力的国家中心城市和粤港澳大湾区核心。", "广州馆未放置灭火器。", "在广州馆发现火源。", "这里就是广州馆啦，我要继续回去工作啦！"}},
    {"jilin",     {"吉林省，简称 “吉”，地处东北中部，与俄、朝接壤。是重要商品粮基地与老工业基地，有长白山等美景，人文风情浓郁。", "吉林馆未放置灭火器。", "在吉林馆发现火源。", "这里就是吉林馆啦，我要继续回去工作啦！"}},
    {"shenzhen",  {"深圳,是广东副省级市、经济特区。毗邻香港,经济发达,创新力强,有众多世界500 强企业，是粤港澳大湾区中心城市。", "深圳馆未放置灭火器。", "在深圳馆发现火源。", "这里就是深圳馆啦，我要继续回去工作啦！"}},
    {"shanghai",  {"上海，简称 “沪” 或 “申”，是中国直辖市，位于长江入海口，是国际经济、金融、贸易、航运、科技创新中心，有独特海派文化。", "上海馆未放置灭火器。", "在上海馆发现火源。", "这里就是上海馆啦，我要继续回去工作啦！"}}
};

std::map<std::string, std::string> tts_audio_map; 

void loadTTSAudioMap() {
    // 注意：这里的绝对路径指向您的 src/TTS/ 目录
    std::ifstream file("/home/reicom2025/ros_workspace/src/TTS/TTS.txt");
    if (!file.is_open()) {
        ROS_ERROR("无法打开 TTS.txt，请检查路径是否正确！");
        return;
    }
    
    std::string line;
    int index = 1;
    while (std::getline(file, line)) {
        // 去除 Windows 换行符可能带来的 \r
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue; // 跳过空行

        // 智能清洗：如果行首带有 "1. ", "2: " 等序号，自动剥离
        size_t sep_pos = std::string::npos;
        for (size_t i = 0; i < line.length() && i < 5; ++i) {
            if (line[i] == '.' || line[i] == ':' || line[i] == ' ') {
                sep_pos = i;
                break;
            }
            if (!std::isdigit(line[i])) break;
        }
        
        std::string clean_text = line;
        if (sep_pos != std::string::npos) {
            clean_text = line.substr(sep_pos + 1);
            while (!clean_text.empty() && clean_text.front() == ' ') clean_text.erase(0, 1);
        }

        // 建立映射：文本 -> X.mp3 (路径拼接在 speak 函数中进行)
        if (!clean_text.empty()) {
            tts_audio_map[clean_text] = std::to_string(index) + ".mp3";
            index++;
        }
    }
    ROS_INFO("======================================================");
    ROS_INFO(" TTS 语音库加载完毕，共加载 %lu 条语音映射！", tts_audio_map.size());
    ROS_INFO("======================================================");
}


// ==========================================
// 4. 全局控制变量
// ==========================================
RobotState current_state = STATE_IDLE;
ros::Publisher nav_cmd_pub;         // 向全能司机发送场馆字符串的话题
ros::ServiceClient audio_srv_client;// 官方 AIUI 录音控制服务客户端
ros::Publisher charge_cmd_pub;      // 用于向充电节点下发开始指令
ros::Subscriber charge_status_sub;  // 用于接收充电节点的完成汇报

// 任务顺序控制标志：0表示允许执行任务一，1表示任务一已执行且被锁定
int task_sequence_flag = 0; 

// 任务一巡检专属变量
std::string current_t1_destination = ""; // 记录当前任务一的动态目的地

// 任务二巡检专属变量
std::vector<std::string> inspect_targets = {"jilin", "guangzhou", "beijing", "shenzhen", "shanghai"};
int current_inspect_idx = 0;
bool yolo_fire_detected = false;          // 模拟订阅到的 YOLO 火源标志
bool yolo_extinguisher_missing = false;   // 模拟订阅到的 YOLO 缺少灭火器标志

// 用于非阻塞等待 YOLO 结果的变量
ros::Time room_arrive_time;
bool is_checking_yolo = false;

// ==========================================
// 5. 核心工具函数
// ==========================================
// 语音合成 (TTS) 执行器
void speak(const std::string& text) {
    ROS_INFO("[语音播报] -> %s", text.c_str());
    
    auto it = tts_audio_map.find(text);
    
    if (it != tts_audio_map.end()) {
        // 找到了对应的 MP3，拼接绝对路径并使用 ffplay 播放
        std::string file_path = "/home/reicom2025/ros_workspace/src/TTS/" + it->second;
        std::string cmd = "ffplay -nodisp -autoexit \"" + file_path + "\"";
        
        // ffplay 默认阻塞，播完才往下走，节奏完美，无需手动 sleep
        system(cmd.c_str()); 
        
    } else {
        // 兜底方案：文本不匹配时，使用 espeak
        ROS_WARN("! 未在 TTS.txt 中找到完全匹配的文本，启用 espeak 兜底播报！");
        std::string cmd = "espeak -v zh+f2 -s 170 \"" + text + "\" &";
        system(cmd.c_str());
        ros::Duration(text.length() * 0.15).sleep(); 
    }
}

// 控制官方麦克风录音开关
void toggleAudioRecording(bool start) {
    if (!audio_srv_client.waitForExistence(ros::Duration(2.0))) {
        ROS_WARN("! 录音服务不可用，请确认 launch 是否加载语音服务");
        return;
    }
    std_srvs::SetBool srv;
    srv.request.data = start;
    if (audio_srv_client.call(srv)) {
        ROS_INFO("收音: %s", start ? "【开启】" : "【关闭】");
    }
}

// ==========================================
// 6. 话题回调接收函数 (打通视觉与听觉)
// ==========================================
// A. 接收人脸检测结果
void faceCallback(const std_msgs::String::ConstPtr& msg) {
    if (current_state != STATE_IDLE) return; // 只有在闲置待机时才接受脸部唤醒

    std::string name = msg->data;
    ROS_INFO("[视觉] 眼前出现目标: %s", name.c_str());

    if (name == "normal_visitor" || name == "visitor") {
        // 【新增拦截逻辑】：如果任务一已被锁定，则拒绝触发并提示
        if (task_sequence_flag == 1) {
            ROS_WARN("任务一已被锁定，必须先触发任务二才能再次执行任务一！");
            return; // 直接中断，不改变状态机状态
        }

        // 【触发任务一】迎宾机器人开发
        speak(VOICE_GREETING);
        current_state = STATE_T1_WAIT_VOICE;
        toggleAudioRecording(true); // 开启听觉，等待命令
    } 
    else if (name == "Juwan" || name == "Glenn" || name == "小明") {
        // 【新增解锁逻辑】：触发任务二，同时解除任务一的锁定
        task_sequence_flag = 0; // 重置为0，允许后续再次触发任务一

        // 【触发任务二】巡检机器人开发
        std::string admin_name = (name == "小明") ? "周晓铭" : name;
        speak("你好，管理员翁佳亮");
        current_state = STATE_T2_WAIT_START;
        toggleAudioRecording(true); // 开启听觉，等待开始口令
    }
}

// B. 接收语音识别文本结果
void voiceTextCallback(const std_msgs::String::ConstPtr& msg) {
    std::string text = msg->data;
    ROS_INFO("[听觉] 解析出文字: %s", text.c_str());

    if (current_state == STATE_T1_WAIT_VOICE) {
        // 状态机处理：当前处于任务一等待目的地语音状态
        static const std::map<std::string, Destination> target_map = {
            {"深圳", {"shenzhen",  "深圳馆"}},
            {"北京", {"beijing",   "北京馆"}},
            {"广州", {"guangzhou", "广州馆"}},
            {"吉林", {"jilin",     "吉林馆"}},
            {"上海", {"shanghai",  "上海馆"}}
        };

        bool is_matched = false;

        // 核心遍历逻辑：精准剥离城市关键词
        for (const auto& pair : target_map) {
            if (text.find(pair.first) != std::string::npos) {
                
                toggleAudioRecording(false);  // 1. 停止收音，专心发车
                system("ffplay -nodisp -autoexit /home/reicom2025/ros_workspace/src/TTS/2.mp3");    // 2. 播报语音：“好的，请跟我来。”

                // 【关键】记录当前任务一的目的地代号，供到达后播报使用
                current_t1_destination = pair.second.cmd;
                
                // 3. 动态给全能老司机下发目标指令
                std_msgs::String cmd_msg;
                cmd_msg.data = pair.second.cmd;
                nav_cmd_pub.publish(cmd_msg);
                
                // 4. 跃迁状态机状态，锁死后续干扰
                current_state = STATE_T1_NAVIGATING;
                ROS_INFO("状态切入: [前往%s]", pair.second.name.c_str()); 
                
                is_matched = true;
                break; 
            }
        }

        // 优化 2：完善防卡死兜底逻辑。
        if (!is_matched && (text.find("参观") != std::string::npos || text.find("去") != std::string::npos)) {
            ROS_WARN("听到了引导意图，但未能匹配城市关键词。触发重新倾听引导...");
        }
    }
    
    else if (current_state == STATE_T2_WAIT_START) {
        // 状态机处理：任务二等待巡检启动口令
        if (text.find("开始") != std::string::npos || text.find("巡检") != std::string::npos) {
            toggleAudioRecording(false);
            speak(VOICE_INSPECT_MOD);
            
            // 开始遍历第一个场馆
            current_inspect_idx = 0;
            std_msgs::String cmd_msg;
            cmd_msg.data = inspect_targets[current_inspect_idx];
            nav_cmd_pub.publish(cmd_msg);
            
            current_state = STATE_T2_INSPECTING;
            ROS_INFO("状态切入: [任务二场馆智能巡检中...]");
        }
    }
}

// C. 接收导航状态结果反馈
void navStatusCallback(const std_msgs::String::ConstPtr& msg) {
    std::string status = msg->data;
    if (status != "ARRIVED") return;

    // 状态机处理：任务一到达目标馆
    if (current_state == STATE_T1_NAVIGATING) {
        current_state = STATE_T1_ARRIVED_DES;

        // 【修改】动态打印到达的展馆名称
        ROS_INFO("状态切入: [到达 %s 馆，开始宣讲]", current_t1_destination.c_str());
        
        // 【修改】根据动态记录的目的地，从字典中提取对应的 intro 和 goodbye
        speak(pavilion_dict[current_t1_destination].intro);
        ros::Duration(1.0).sleep();
        speak(pavilion_dict[current_t1_destination].goodbye);

        // 【新增上锁逻辑】：任务一流程结束，锁定任务一，防止重复触发
        task_sequence_flag = 1;
        ROS_INFO("任务一完成，任务一已被锁定，需等待任务二触发后解锁。");
        
        // 自动切入返回出发区命令
        ROS_INFO("任务一结束，命令司机返回出发区...");
        std_msgs::String cmd_msg;
        cmd_msg.data = "start";
        nav_cmd_pub.publish(cmd_msg);
        
        current_state = STATE_RETURN_HOME;
    }
    // 状态机处理：任务二巡检中到达了某一个馆
    else if (current_state == STATE_T2_INSPECTING) {
        std::string current_room = inspect_targets[current_inspect_idx];
        ROS_INFO("抵达巡检区: [%s] 馆，开始进行资产与安全隐患排查...", current_room.c_str());
    
        // 【修复】1. 重置标志位，防止上一个馆的状态污染当前馆
        yolo_fire_detected = false;
        yolo_extinguisher_missing = true;

        // 【修复】2. 使用 spinOnce 循环等待，避免阻塞 ROS 消息队列
        ros::Time start_time = ros::Time::now();
        while (ros::ok() && (ros::Time::now() - start_time).toSec() < 3.0) {
            ros::spinOnce(); // 允许在等待期间处理 YOLO 回调
            ros::Duration(0.1).sleep();
    }

    // 检查火源隐患
    if (yolo_fire_detected) {
        ROS_WARN("发现火源！播放警报音频...");
        system("ffplay -nodisp -autoexit /home/reicom2025/ros_workspace/警报声.mp3");
        speak(pavilion_dict[current_room].fire_found);
    }
    if (yolo_extinguisher_missing) {
        ROS_WARN("缺少灭火器！播放警报音频...");
        system("ffplay -nodisp -autoexit /home/reicom2025/ros_workspace/警报声.mp3");
        speak(pavilion_dict[current_room].no_extinguisher);
    }

        // 场馆切换逻辑
        current_inspect_idx++;
        if (current_inspect_idx < inspect_targets.size()) {
            // 还有场馆没巡检完，命令司机去下一个馆
            ROS_INFO("前往下一个巡检目的地...");
            std_msgs::String cmd_msg;
            cmd_msg.data = inspect_targets[current_inspect_idx];
            nav_cmd_pub.publish(cmd_msg);
        } else {
            // 所有场馆遍历完毕，触发最终的充电大招
            ROS_INFO(" 所有场馆智能巡检完毕！启动自动充电...");
            std_msgs::String charge_msg;
            charge_msg.data = "START_CHARGE";
            charge_cmd_pub.publish(charge_msg); // 通知充电节点开始干活
            
            current_state = STATE_T2_CHARGING;  // 大脑进入挂机等待状态
        }
    }
    // 终点连招：安全回到出发点，绿灯亮起，重置大脑
    else if (current_state == STATE_RETURN_HOME) {
        ROS_INFO("🏆 机器人已成功完成任务并返回初始点！系统重置待机。");
        current_state = STATE_IDLE;
    }
} 

// D. 专门接收充电包反馈的回调函数
void chargeStatusCallback(const std_msgs::String::ConstPtr& msg) {
    if (current_state != STATE_T2_CHARGING) return;

    if (msg->data == "CHARGE_DONE") {
        ROS_INFO("收到充电完成信号！下发返回初始点指令...");
        

        std_msgs::String cmd_msg;
        cmd_msg.data = "start"; // 司机（nav_goal.cpp）开回起点
        nav_cmd_pub.publish(cmd_msg);

        current_state = STATE_RETURN_HOME;
    }
}

// E. 接收 YOLO 视觉检测结果
void yoloResultCallback(const std_msgs::String::ConstPtr& msg) {
    std::string result = msg->data;
    if (result == "fire") {
        yolo_fire_detected = true;
    }
    // 【关键修复】只要视觉节点发来了 extinguisher，说明看到了，立刻将缺失标志位置为 false
    if (result == "extinguisher") {
        yolo_extinguisher_missing = false; 
    }
}

// ==========================================
// 7. 主函数
// ==========================================
int main(int argc, char** argv) {
    setlocale(LC_CTYPE, "zh_CN.utf8");
    ros::init(argc, argv, "robocom_master_brain");
    ros::NodeHandle nh;

    loadTTSAudioMap();

    // 发布者：下发语义指令给司机
    nav_cmd_pub = nh.advertise<std_msgs::String>("/voice_nav_cmd", 10);

    // 服务客户端：控制官方 AIUI 麦克风录音
    audio_srv_client = nh.serviceClient<std_srvs::SetBool>("/REIService/RecordAudio");

    // 订阅者们：收听眼睛、耳朵、司机的动向
    ros::Subscriber sub_face = nh.subscribe("/face_result", 10, faceCallback);
    ros::Subscriber sub_voice = nh.subscribe("/vosk_result", 10, voiceTextCallback); 
    ros::Subscriber sub_nav = nh.subscribe("/nav_driver_status", 10, navStatusCallback);
    ros::Subscriber sub_yolo = nh.subscribe("/yolo_detection_result", 10, yoloResultCallback);

    charge_cmd_pub = nh.advertise<std_msgs::String>("/charge_cmd", 10);
    charge_status_sub = nh.subscribe("/charge_status", 10, chargeStatusCallback);

    ROS_INFO("======================================================");
    ROS_INFO(" 睿抗机器人状态机加载成功！当前状态: [出发区待机] ");
    ROS_INFO("======================================================");

    ros::spin();
    return 0;
}