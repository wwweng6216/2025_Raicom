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

    // 辅助结构体，存储全套 4 参数坐标
    struct PrecisePose {
        double x, y, z, w;
    };

public:
    NavDriver() : ac_("move_base", true) {
        cmd_sub_ = nh_.subscribe("/voice_nav_cmd", 1, &NavDriver::cmdCallback, this);
        status_pub_ = nh_.advertise<std_msgs::String>("/nav_driver_status", 10); // 已修正

        ROS_INFO("等待连接 move_base 服务器...");
        ac_.waitForServer();
        ROS_INFO("全能老司机已连接 move_base 导航服务器，整装待发！");
    }

    void cmdCallback(const std_msgs::String::ConstPtr& msg) {
        std::string goal_place = msg->data;
        ROS_INFO("老司机收到目的地指令: %s", goal_place.c_str());

        move_base_msgs::MoveBaseGoal goal;
        goal.target_pose.header.frame_id = "map";
        goal.target_pose.header.stamp = ros::Time::now();

        PrecisePose target = {0.0, 0.0, 0.0, 1.0};
        bool valid_target = false;

        // 【闭环正赛】：完美对齐你提取到的 4 参数真实物理世界坐标
        if (goal_place == "shenzhen") {
            target = {0.997, 1.218, -0.004, 1.000};
            valid_target = true;
        } 
        else if (goal_place == "start" || goal_place == "home") {
            target = {0.026, -0.008, 0.737, 0.676};
            valid_target = true;
        }
        else if (goal_place == "beijing") {
            target = {2.444, 0.195, 0.008, 1.000};
            valid_target = true;
        }
        else if (goal_place == "guangzhou") {
            target = {2.474, 1.162, 0.011, 1.000};
            valid_target = true;
        }
        else if (goal_place == "jilin") {
            target = {2.480, 2.181, 0.015, 1.000};
            valid_target = true;
        }
        else if (goal_place == "shanghai") {
            target = {1.051, 2.187, 0.007, 1.000};
            valid_target = true;
        }

        if (!valid_target) {
            ROS_WARN("未知目的地: %s，拒绝发车！", goal_place.c_str());
            return;
        }

        // 装填标准 2D 导航弹药包
        goal.target_pose.pose.position.x = target.x;
        goal.target_pose.pose.position.y = target.y;
        goal.target_pose.pose.position.z = 0.0; 

        goal.target_pose.pose.orientation.x = 0.0;
        goal.target_pose.pose.orientation.y = 0.0;
        goal.target_pose.pose.orientation.z = target.z; 
        goal.target_pose.pose.orientation.w = target.w; 

        ROS_INFO("正在奔赴目的地 [%s]...", goal_place.c_str());
        ac_.sendGoal(goal);

        ac_.waitForResult();

        if (ac_.getState() == actionlib::SimpleClientGoalState::SUCCEEDED) {
            ROS_INFO("顺利抵达 [%s] ！正在向大脑汇报...", goal_place.c_str());
            std_msgs::String status_msg;
            status_msg.data = "ARRIVED";
            status_pub_.publish(status_msg);
        } else {
            ROS_ERROR("导航去 [%s] 失败或被中止！", goal_place.c_str());
        }
    }
};

int main(int argc, char** argv) {
    setlocale(LC_CTYPE, "zh_CN.utf8");
    ros::init(argc, argv, "reicoures_nav_node");
    NavDriver driver;
    ros::spin();
    return 0;
}