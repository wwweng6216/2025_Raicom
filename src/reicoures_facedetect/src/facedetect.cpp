#include <ros/ros.h>
#include "face_rec/recognition_results.h"

#include <string>
#include <vector>

using namespace std;

// 创建服务客户端
ros::ServiceClient facedet_client;

vector<string> face_detect()
{
    vector<string> labels;
    // 等待服务上线
    ROS_INFO("等待服务 /face_recognition_results 启动...");
    facedet_client.waitForExistence();
    ROS_INFO("服务已连接！");

    face_rec::recognition_results srv;
    srv.request.mode = 1;
    srv.request.str = "/head_camera/image_raw";
    // 发送请求
    if (facedet_client.call(srv))
    {
        if (srv.response.success)
        {
            // 遍历识别结果
            for (auto &face_data : srv.response.result.face_data)
            {
                string label = face_data.header.frame_id;
                labels.push_back(label);
                ROS_INFO("识别到了：%s", label.c_str());
            }
            return labels;
        }
        else
        {
            ROS_ERROR("人脸识别失败：%s", srv.response.message.c_str());
            return labels;
        }
    }
    else
    {
        ROS_ERROR("服务调用失败！");
        return labels;
    }
}


int main(int argc, char** argv)
{
    setlocale(LC_CTYPE, "zh_CN.utf8");
    ros::init(argc, argv, "face_detect_node");
    ros::NodeHandle nh;
    facedet_client = nh.serviceClient<face_rec::recognition_results>("/face_recognition_results");
    // 调用识别
    vector<string> labels = face_detect();

    // 判断结果
    if (!labels.empty())
    {
        // 查找是否包含“Glenn”
        bool found = false;
        for (string &name : labels)
        {
            if (name == "Glenn")
            {
                found = true;
                break;
            }
        }

        if (found)
        {
            ROS_INFO("您好,Glenn");
        }
        else
        {
            ROS_WARN("验证失败");
        }
    }
    else
    {
        ROS_WARN("请面向采集相机");
    }

    return 0;

}