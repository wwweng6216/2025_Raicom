#include <ros/ros.h>
#include <std_msgs/String.h>
#include <std_srvs/SetBool.h>
#include <iostream>
#include <string>
#include <map>
#include <vector>

// ==========================================
// 1. 状态机枚举定义 (涵盖任务一与任务二)
// ==========================================
enum RobotState {
    STATE_IDLE,                 // 待机状态：在出发区等待人脸检测
    STATE_T1_WAIT_VOICE,        // 任务一：已热烈欢迎，等待评委说出目的地
    STATE_T1_NAVIGATING,        // 任务一：开车导览前往深圳馆中
    STATE_T1_ARRIVED_DES,       // 任务一：到达深圳馆，播放介绍和结束语
    STATE_T2_WAIT_START,        // 任务二：认出管理员，等待说“开始执行巡检任务”
    STATE_T2_INSPECTING,        // 任务二：智能巡检遍历场馆中
    STATE_RETURN_HOME           // 终点连招：返回出发区并触发 AR 精准充电
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

const std::string VOICE_GREETING    = "你好，欢迎您的到来！有什么需要帮助的吗？";
const std::string VOICE_GUIDE_START = "好的，请跟我来。";
const std::string VOICE_INSPECT_MOD = "好的，进入巡检模式。";

std::map<std::string, PavilionVoice> pavilion_dict = {
    {"beijing",   {"北京，中国首都，千年古都与现代都市交融，尽显独特魅力。这里有宏伟的故宫、绵延的长城等历史古迹，见证着岁月的沧桑变迁。", "北京馆未放置灭火器。", "在北京馆发现火源。", "这里就是北京馆啦，我要继续回去工作啦！"}},
    {"guangzhou", {"广州，别称羊城、花城，广东省会。历史悠久，美食诱人，经济发达，是充满魅力与活力的国家中心城市和粤港澳大湾区核心。", "广州馆未放置灭火器。", "在广州馆发现火源。", "这里就是广州馆啦，我要继续回去工作啦！"}},
    {"jilin",     {"吉林省，简称 “吉”，地处东北中部，与俄、朝接壤。是重要商品粮基地与老工业基地，有长白山等美景，人文风情浓郁。", "吉林馆未放置灭火器。", "在吉林馆发现火源。", "这里就是吉林馆啦，我要继续回去工作啦！"}},
    {"shenzhen",  {"深圳,是广东副省级市、经济特区。毗邻香港,经济发达,创新力强,有众多世界500 强企业，是粤港澳大湾区中心城市。", "深圳馆未放置灭火器。", "在深圳馆发现火源。", "这里就是深圳馆啦，我要继续回去工作啦！"}},
    {"shanghai",  {"上海，简称 “沪” 或 “申”，是中国直辖市，位于长江入海口，是国际经济、金融、贸易、航运、科技创新中心，有独特海派文化。", "上海馆未放置灭火器。", "在上海馆发现火源。", "这里就是上海馆啦，我要继续回去工作啦！"}}
};

// ==========================================
// 3. 全局控制变量
// ==========================================
RobotState current_state = STATE_IDLE;
ros::Publisher nav_cmd_pub;         // 向全能司机发送场馆字符串的话题
ros::ServiceClient audio_srv_client;// 官方 AIUI 录音控制服务客户端

// 任务二巡检专属变量
std::vector<std::string> inspect_targets = {"beijing", "guangzhou", "jilin", "shanghai", "shenzhen"};
int current_inspect_idx = 0;
bool yolo_fire_detected = false;          // 模拟订阅到的 YOLO 火源标志
bool yolo_extinguisher_missing = false;   // 模拟订阅到的 YOLO 缺少灭火器标志

// ==========================================
// 4. 核心工具函数
// ==========================================
// 语音合成 (TTS) 执行器
void speak(const std::string& text) {
    ROS_INFO("🤖 [语音播报] -> %s", text.c_str());
    // 比赛现场可以直接用 Linux 系统的 espeak 命令，或者调用官方的语音合成话题
    std::string cmd = "espeak -v zh+f2 -s 170 \"" + text + "\" &";
    system(cmd.c_str());
    // 给播报预留合理的物理等待时间（按字数估算延迟）
    ros::Duration(text.length() * 0.15).sleep();
}

// 控制官方麦克风录音开关
void toggleAudioRecording(bool start) {
    if (!audio_srv_client.waitForExistence(ros::Duration(2.0))) {
        ROS_WARN("⚠️ 录音服务不可用，请确认 launch 是否加载语音服务");
        return;
    }
    std_srvs::SetBool srv;
    srv.request.data = start;
    if (audio_srv_client.call(srv)) {
        ROS_INFO("🎤 大脑控制底层收音: %s", start ? "【开启】" : "【关闭】");
    }
}

// ==========================================
// 5. 话题回调接收函数 (打通视觉与听觉)
// ==========================================
// A. 接收人脸检测结果
void faceCallback(const std_msgs::String::ConstPtr& msg) {
    if (current_state != STATE_IDLE) return; // 只有在闲置待机时才接受脸部唤醒

    std::string name = msg->data;
    ROS_INFO("👁️ [视觉发现] 眼前出现目标: %s", name.c_str());

    if (name == "normal_visitor" || name == "visitor") {
        // 【触发任务一】迎宾机器人开发
        speak(VOICE_GREETING);
        current_state = STATE_T1_WAIT_VOICE;
        toggleAudioRecording(true); // 开启听觉，等待命令
    } 
    else if (name == "周晓铭" || name == "霍稷" || name == "小明") {
        // 【触发任务二】巡检机器人开发
        std::string admin_name = (name == "小明") ? "周晓铭" : name;
        speak("你好，管理员" + admin_name + "。");
        current_state = STATE_T2_WAIT_START;
        toggleAudioRecording(true); // 开启听觉，等待开始口令
    }
}

// B. 接收语音识别文本结果
void voiceTextCallback(const std_msgs::String::ConstPtr& msg) {
    std::string text = msg->data;
    ROS_INFO("🎤 [听觉捕获] 解析出文字: %s", text.c_str());

    if (current_state == STATE_T1_WAIT_VOICE) {
        // 状态机处理：当前处于任务一等待目的地语音状态
        // 这里声明为静态常量，保证只在程序启动时初始化一次，避免每次进语音回调都重新在内存构造 Map，极大节省算力。
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
            // pair.first 为城市名（如"北京"），pair.second 为对应的代号和中文名
            if (text.find(pair.first) != std::string::npos) {
                
                toggleAudioRecording(false);  // 1. 停止收音，专心发车
                speak(VOICE_GUIDE_START);     // 2. 播报语音：“好的，请跟我来。”
                
                // 3. 动态给全能老司机下发目标指令
                std_msgs::String cmd_msg;
                cmd_msg.data = pair.second.cmd;
                nav_cmd_pub.publish(cmd_msg);
                
                // 4. 跃迁状态机状态，锁死后续干扰
                current_state = STATE_T1_NAVIGATING;
                ROS_INFO("🔄 状态切入: [前往%s]", pair.second.name.c_str()); 
                
                is_matched = true;
                break; // 成功匹配目的地，立刻切断循环，防止多城市词串扰
            }
        }

        // 优化 2：完善防卡死兜底逻辑。
        // 如果听到了“参观”或者“带我去”，但是城市名因为杂音没听清，必须主动回应引导，否则机器人会变木头。
        if (!is_matched && (text.find("参观") != std::string::npos || text.find("去") != std::string::npos)) {
            ROS_WARN("听到了引导意图，但未能匹配城市关键词。触发重新倾听引导...");
            
            speak("对不起，请问您想去哪个展馆？请清晰地说出城市名称。");
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
            ROS_INFO("🔄 状态切入: [任务二场馆智能巡检中...]");
        }
    }
}

// 接收导航状态结果反馈
void navStatusCallback(const std_msgs::String::ConstPtr& msg) {
    std::string status = msg->data;
    if (status != "ARRIVED") return;

    // 状态机处理：任务一到达目标馆
    if (current_state == STATE_T1_NAVIGATING) {
        current_state = STATE_T1_ARRIVED_DES;
        ROS_INFO("🔄 状态切入: [到达深圳馆，开始宣讲]");
        
        speak(pavilion_dict["shenzhen"].intro);
        ros::Duration(1.0).sleep();
        speak(pavilion_dict["shenzhen"].goodbye);
        
        // 自动切入返回出发区命令
        ROS_INFO("🔄 任务一结束，命令司机返回出发区...");
        std_msgs::String cmd_msg;
        cmd_msg.data = "start";
        nav_cmd_pub.publish(cmd_msg);
        
        current_state = STATE_RETURN_HOME;
    }
    // 状态机处理：任务二巡检中到达了某一个馆
    else if (current_state == STATE_T2_INSPECTING) {
        std::string current_room = inspect_targets[current_inspect_idx];
        ROS_INFO("🔍 抵达巡检区: [%s] 馆，开始进行资产与安全隐患排查...", current_room.c_str());
        
        // 现场留出几秒钟给 YOLO 视觉节点稳定识别
        ros::Duration(3.0).sleep(); 
        
        // 检查火源隐患
        if (yolo_fire_detected) {
            ROS_WARN("🚨 发现火源！播放警报音频...");
            system("play_alarm_sound_4s.sh"); // 模拟播放警报 4 秒
            ros::Duration(4.0).sleep();
            speak(pavilion_dict[current_room].fire_found);
        }
        
        // 检查灭火器资产
        if (yolo_extinguisher_missing) {
            ROS_WARN("🚨 发现资产缺失！播放警报音频...");
            system("play_alarm_sound_4s.sh"); 
            ros::Duration(4.0).sleep();
            speak(pavilion_dict[current_room].no_extinguisher);
        }

        // 场馆切换逻辑
        current_inspect_idx++;
        if (current_inspect_idx < inspect_targets.size()) {
            // 还有场馆没巡检完，命令司机去下一个馆
            ROS_INFO("🚗 前往下一个巡检目的地...");
            std_msgs::String cmd_msg;
            cmd_msg.data = inspect_targets[current_inspect_idx];
            nav_cmd_pub.publish(cmd_msg);
        } else {
            // 所有场馆遍历完毕，触发最终的充电大招
            ROS_INFO("🎉 所有场馆智能巡检完毕！下发一键自主充电指令...");
            std_msgs::String cmd_msg;
            cmd_msg.data = "start"; // 司机收到 start 会自动触发第四课的 AR 对准倒车合体
            nav_cmd_pub.publish(cmd_msg);
            
            current_state = STATE_RETURN_HOME;
        }
    }
    // 终点连招：安全回到出发点，绿灯亮起，重置大脑
    else if (current_state == STATE_RETURN_HOME) {
        ROS_INFO("🏆 [大获全胜] 机器人已成功完成任务并合体充电！系统重置待机。");
        current_state = STATE_IDLE;
    }
}

// ==========================================
// 6. 主函数
// ==========================================
int main(int argc, char** argv) {
    setlocale(LC_CTYPE, "zh_CN.utf8");
    ros::init(argc, argv, "robocom_master_brain");
    ros::NodeHandle nh;

    // 发布者：下发语义指令给司机
    nav_cmd_pub = nh.advertise<std_msgs::String>("/voice_nav_cmd", 10);

    // 服务客户端：控制官方 AIUI 麦克风录音
    audio_srv_client = nh.serviceClient<std_srvs::SetBool>("/REIService/RecordAudio");

    // 订阅者们：收听眼睛、耳朵、司机的动向
    ros::Subscriber sub_face = nh.subscribe("/face_result", 10, faceCallback);
    ros::Subscriber sub_voice = nh.subscribe("/vosk_result", 10, voiceTextCallback); // 对接官方或Vosk文本话题
    ros::Subscriber sub_nav = nh.subscribe("/nav_driver_status", 10, navStatusCallback);

    ROS_INFO("======================================================");
    ROS_INFO(" 🏆 睿抗机器人总控状态机大脑加载成功！当前状态: [出发区待机] ");
    ROS_INFO("======================================================");

    ros::spin();
    return 0;
}