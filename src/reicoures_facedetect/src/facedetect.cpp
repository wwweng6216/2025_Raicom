#include <ros/ros.h>
#include "face_rec/recognition_results.h"
#include <std_msgs/String.h> 
#include <string>
#include <vector>

using namespace std;

ros::ServiceClient facedet_client;
ros::Publisher face_pub; 

void face_detect_and_publish()
{
    if (!facedet_client.exists()) {
        ROS_INFO_THROTTLE(5, "等待服务 /face_recognition_results 启动...");
        return;
    }

    face_rec::recognition_results srv;
    srv.request.mode = 1;
    srv.request.str = "/head_camera/image_raw";

    if (facedet_client.call(srv))
    {
        if (srv.response.success && !srv.response.result.face_data.empty())
        {
            for (auto &face_data : srv.response.result.face_data)
            {
                string label = face_data.header.frame_id;
                ROS_INFO("检测到人脸：%s", label.c_str());

                // 【已修正】：从 std::msgs 恢复为标准 ROS 的 std_msgs
                std_msgs::String msg;
                if (label == "Glenn" || label == "visitor" || label == "normal_visitor") {
                    msg.data = "visitor"; 
                } else if (label == "Juwan" || label == "杨迪喻" ) {
                    msg.data = label;     
                } else {
                    msg.data = "visitor"; 
                }
                
                face_pub.publish(msg);
                break; 
            }
        }
    }
    else
    {
        ROS_ERROR_THROTTLE(5, "人脸识别服务调用失败！");
    }
}

int main(int argc, char** argv)
{
    setlocale(LC_CTYPE, "zh_CN.utf8");
    ros::init(argc, argv, "face_detect_node");
    ros::NodeHandle nh;

    facedet_client = nh.serviceClient<face_rec::recognition_results>("/face_recognition_results");
    face_pub = nh.advertise<std_msgs::String>("/face_result", 10);

    ros::Rate rate(2); 
    while (ros::ok())
    {
        face_detect_and_publish();
        ros::spinOnce();
        rate.sleep();
    }
    return 0;
}