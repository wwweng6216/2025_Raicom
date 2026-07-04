#include <std_msgs/String.h>
#include <ros/ros.h>
#include "relative_move/SetRelativeMove.h"
#include <actionlib/client/simple_action_client.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <ar_pose/Track.h>

// 创建服务客户端
ros::ServiceClient relmove_client;
ros::ServiceClient track_client; 

// 增加一个全局的发布者，用于向大脑汇报
ros::Publisher status_pub;

// 定义 Action 客户端类型
typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;
MoveBaseClient* nav_client;

bool set_ARtrack(int id, float dist){
    // 等待服务上线
    ROS_INFO("等待服务 /track 启动...");
    track_client.waitForExistence();
    ROS_INFO("服务已连接！");
    ar_pose::Track srv;
    srv.request.ar_id = id;
    srv.request.goal_dist = dist;
    //发送请求
    if (track_client.call(srv)){
        if (srv.response.success){
            ROS_INFO("二次定位成功：%s", srv.response.message.c_str());
            return 1;
        }else{
            ROS_ERROR("二次定位失败：%s", srv.response.message.c_str());
            return 0;
        }
    }else{
        ROS_ERROR("track服务调用失败！");
        return 0;
    }
}

bool set_relmove(float x,float y,float theta){
    // 等待服务上线
    ROS_INFO("等待服务 /relative_move 启动...");
    relmove_client.waitForExistence();
    ROS_INFO("服务已连接！");
    // 定义服务消息
    relative_move::SetRelativeMove srv;

    // 填充请求数据
    srv.request.goal.x = x;
    srv.request.goal.y = y;
    srv.request.goal.theta = theta;
    srv.request.global_frame = "odom";
    // 发送请求
    if (relmove_client.call(srv)){
        if (srv.response.success){
            ROS_INFO("移动成功：%s", srv.response.message.c_str());
            return 1;
        }else{
            ROS_ERROR("移动失败：%s", srv.response.message.c_str());
            return 0;
        }
    }else{
        ROS_ERROR("服务调用失败！");
        return 0;
    }
}


bool navToGoal(double x, double y, double z, double w){
    // 等待服务器连接成功
    ROS_INFO("等待连接 move_base 服务器...");
    nav_client->waitForServer();
    ROS_INFO("连接成功！");
    nav_client->cancelAllGoals();
    ROS_WARN("已清空所有导航任务！");
    // 构造导航目标消息
    move_base_msgs::MoveBaseGoal goal;

    // 设置坐标系为 map
    goal.target_pose.header.frame_id = "map";
    goal.target_pose.header.stamp = ros::Time::now();

    // 设置目标坐标（可修改 x, y）
    goal.target_pose.pose.position.x = x;
    goal.target_pose.pose.position.y = y;

    // 设置朝向
    goal.target_pose.pose.orientation.z = z;
    goal.target_pose.pose.orientation.w = w;
    // 发送目标点
    ROS_INFO("发送导航目标...");
    nav_client->sendGoal(goal);

    // 循环监听导航状态
    ros::Rate rate(5);
    while (ros::ok())
    {
        actionlib::SimpleClientGoalState state = nav_client->getState();
        std::string state_str = state.toString();

        // 实时打印状态
        ROS_INFO("当前导航状态：%s", state_str.c_str());

        // 导航成功
        if (state == actionlib::SimpleClientGoalState::SUCCEEDED)
        {
            ROS_INFO("导航成功：已到达目标点！");
            return true;
        }
        // 导航失败（内部错误/障碍物/无法规划）
        else if (state == actionlib::SimpleClientGoalState::ABORTED)
        {
            ROS_ERROR("导航失败：无法到达目标！");
            return false;
        }
        // 任务被取消
        else if (state == actionlib::SimpleClientGoalState::PREEMPTED)
        {
            ROS_WARN("导航任务已被取消！");
            return false;
        }
        // 任务被拒绝
        else if (state == actionlib::SimpleClientGoalState::REJECTED)
        {
            ROS_ERROR("导航目标被服务器拒绝！");
            return false;
        }
        // 导航超时
        else if (state == actionlib::SimpleClientGoalState::LOST)
        {
            ROS_ERROR("导航连接丢失！");
            return false;
        }
        rate.sleep();
    }
    // ROS 退出
    return false;
}

void chargeCommandCallback(const std_msgs::String::ConstPtr& msg) {
    if (msg->data == "START_CHARGE") {
        ROS_INFO(">> 收到大脑指令，重定位包接管底盘，开始充电流程...");

        if (!navToGoal(0.3, 1.95, -0.7, 0.7)) {
            ROS_ERROR("无法到达充电桩预备位！");
            return; 
        }

        if (!set_ARtrack(1, 0.4)) {
            ROS_ERROR("AR 对准失败！");
            return;
        }

        if (!set_relmove(-0.18, 0, 0)) return;

        ROS_INFO(">> 正在充电中...");
        ros::Duration(4.0).sleep(); 

        if (!set_relmove(0.18, 0, 0)) return;

        ROS_INFO(">> 成功脱离充电桩，向大脑发送完成信号...");
        std_msgs::String done_msg;
        done_msg.data = "CHARGE_DONE";
        status_pub.publish(done_msg);
    }
}

// ==========================================
// 主函数
// ==========================================
int main(int argc, char** argv)
{
    setlocale(LC_CTYPE, "zh_CN.utf8");
    ros::init(argc, argv, "relocalization_node");
    ros::NodeHandle nh;

    relmove_client = nh.serviceClient<relative_move::SetRelativeMove>("/relative_move");
    track_client = nh.serviceClient<ar_pose::Track>("/track");
    nav_client = new MoveBaseClient("move_base", true);

    // 【1. 必须新增】订阅大脑的命令。只有听到 "START_CHARGE" 才会去干活
    ros::Subscriber cmd_sub = nh.subscribe("/charge_cmd", 1, chargeCommandCallback);
    
    // 【2. 必须新增】初始化状态发布者，干完活才能向大脑汇报 "CHARGE_DONE"
    status_pub = nh.advertise<std_msgs::String>("/charge_status", 1);

    ROS_INFO("===========================================");
    ROS_INFO("重定位充电节点已启动，当前在后台待命...");
    ROS_INFO("等待大脑下发充电指令...");
    ROS_INFO("===========================================");
    
    // 【3. 必须修改】用 spin() 替代你原来那堆直接执行的代码。
    // 这会让程序永远不死，一直在这里听从大脑的调遣！
    ros::spin(); 

    delete nav_client;
    return 0;
}