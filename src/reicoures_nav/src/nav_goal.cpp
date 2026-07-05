#include <ros/ros.h>
#include <actionlib/client/simple_action_client.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <std_msgs/String.h>

typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;

class NavDriver {
private:
    ros::NodeHandle nh_;
    ros::Subscriber cmd_sub_;
    ros::Publisher status_pub_;
    MoveBaseClient ac_;
    bool is_paused_;    // 导航暂停标志位
    std::string current_goal_name_; // 记录当前目标名，供回调打印
    
    struct PrecisePose { double x, y, z, w; };

public:
    NavDriver() : ac_("move_base", true), is_paused_(false) {
        cmd_sub_ = nh_.subscribe("/voice_nav_cmd", 10, &NavDriver::cmdCallback, this);
        status_pub_ = nh_.advertise<std_msgs::String>("/nav_driver_status", 10);
        ROS_INFO("等待连接 move_base 服务器...");
        ac_.waitForServer();
        ROS_INFO("全能老司机已连接 move_base 导航服务器，整装待发！");
    }

    // 【核心修改】：非阻塞的完成回调函数
    void doneCb(const actionlib::SimpleClientGoalState& state, const move_base_msgs::MoveBaseResultConstPtr& result) {
        if (state == actionlib::SimpleClientGoalState::SUCCEEDED) {
            ROS_INFO("顺利抵达 [%s] ！正在向大脑汇报...", current_goal_name_.c_str());
            std_msgs::String status_msg;
            status_msg.data = "ARRIVED";
            status_pub_.publish(status_msg);
        } else {
            ROS_ERROR("导航去 [%s] 失败或被中止！状态: %s", current_goal_name_.c_str(), state.toString().c_str());
        }
    }

    void cmdCallback(const std_msgs::String::ConstPtr& msg) {
        std::string goal_place = msg->data;

        // 1. 接收大脑的“暂停”指令（瞬间响应，不再被阻塞）
        if (goal_place == "PAUSE_NAV") {
            is_paused_ = true;
            ac_.cancelAllGoals(); 
            ROS_WARN(">> 导航已被大脑暂停（充电包接管底盘）！");
            return;
        }
        // 2. 接收大脑的“恢复”指令
        if (goal_place == "RESUME_NAV") {
            is_paused_ = false;
            ROS_INFO(">> 导航已恢复，重新接受大脑指令！");
            return;
        }
        // 3. 拦截暂停期间的普通指令
        if (is_paused_) {
            ROS_WARN("导航处于暂停状态，拒绝执行指令: %s", goal_place.c_str());
            return;
        }

        ROS_INFO("老司机收到目的地指令: %s", goal_place.c_str());
        current_goal_name_ = goal_place;
        
        move_base_msgs::MoveBaseGoal goal;
        goal.target_pose.header.frame_id = "map";
        goal.target_pose.header.stamp = ros::Time::now();
        
        PrecisePose target = {0.0, 0.0, 0.0, 1.0};
        bool valid_target = false;

        // 【闭环正赛】：完美保留你微调过的真实物理世界坐标
        if (goal_place == "shenzhen")      { target = {1.050, 1.217, 0.000, 0.990}; valid_target = true; }
        else if (goal_place == "start" || goal_place == "home") { target = {0.025, 0.000, 0.000, 0.999}; valid_target = true; }
        else if (goal_place == "beijing")  { target = {2.520, 0.207, 0.000, 0.985}; valid_target = true; }
        else if (goal_place == "guangzhou"){ target = {2.500, 1.204, 0.000, 0.975}; valid_target = true; }
        else if (goal_place == "jilin")    { target = {2.490, 2.202, 0.000, 0.985}; valid_target = true; }
        else if (goal_place == "shanghai") { target = {1.050, 2.205, 0.000, 0.965}; valid_target = true; }

        if (!valid_target) {
            ROS_WARN("未知目的地: %s，拒绝发车！", goal_place.c_str());
            return;
        }

        goal.target_pose.pose.position.x = target.x;
        goal.target_pose.pose.position.y = target.y;
        goal.target_pose.pose.position.z = 0.0;
        goal.target_pose.pose.orientation.x = 0.0;
        goal.target_pose.pose.orientation.y = 0.0;
        goal.target_pose.pose.orientation.z = 0.0;
        goal.target_pose.pose.orientation.w = 1.0;

        ROS_INFO("正在奔赴目的地 [%s]...", goal_place.c_str());
        
        // 【核心修改】：使用非阻塞的 sendGoal，绑定 doneCb 回调，彻底抛弃 waitForResult()
        ac_.sendGoal(goal, 
                     boost::bind(&NavDriver::doneCb, this, _1, _2),
                     MoveBaseClient::SimpleActiveCallback(),
                     MoveBaseClient::SimpleFeedbackCallback());
    }
};

int main(int argc, char** argv) {
    setlocale(LC_CTYPE, "zh_CN.utf8");
    ros::init(argc, argv, "reicoures_nav_node");
    NavDriver driver;
    ros::spin();
    return 0;
}