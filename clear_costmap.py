#!/usr/bin/env python
import rospy
from std_srvs.srv import Empty

if __name__ == "__main__":
    rospy.init_node('costmap_cleaner')
    rospy.wait_for_service('/move_base_node/clear_costmaps')
    clear = rospy.ServiceProxy('/move_base_node/clear_costmaps', Empty)
    rate = rospy.Rate(0.3)  # 0.5Hz = 每2秒清理一次（建议不低于0.3Hz）
    
    while not rospy.is_shutdown():
        try:
            clear()
            rospy.loginfo("Costmap cleared")
        except Exception as e:
            rospy.logerr("Failed to clear: %s", e)
        rate.sleep()