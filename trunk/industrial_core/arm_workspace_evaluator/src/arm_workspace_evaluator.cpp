/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2012, Southwest Research Institute
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 	* Redistributions of source code must retain the above copyright
 * 	notice, this list of conditions and the following disclaimer.
 * 	* Redistributions in binary form must reproduce the above copyright
 * 	notice, this list of conditions and the following disclaimer in the
 * 	documentation and/or other materials provided with the distribution.
 * 	* Neither the name of the Southwest Research Institute, nor the names
 *	of its contributors may be used to endorse or promote products derived
 *	from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "ros.h"


double const trans_min = -2.0;
double const trans_max = 2.0;
double const x_min = trans_min;
double const x_max = trans_max;
double const y_min = trans_min;
double const y_max = trans_max;
double const z_min = trans_min;
double const z_max = trans_max;

void evaluate_workspace()
{
  JointTrajPtMessage jMsg;
  SimpleMessage msg;
  SimpleMessage reply;

  ROS_INFO("Joint trajectory downloader: entering stopping state");
  jMsg.point_.setSequence(SpecialSeqValues::STOP_TRAJECTORY);
  jMsg.toRequest(msg);
  ROS_DEBUG("Sending stop command");
  this->robot_->sendAndReceiveMsg(msg, reply);
  ROS_DEBUG("Stop command sent");
}

int main(int argc, char** argv)
{
	const unsigned int IP_ARG_IDX = 1;
	ros::init(argc, argv, "joint_trajectory_handler");
	
	if(argc != 1)  //Only one argument, the robot IP address is accepted
	{
		ROS_INFO("Motion download interface connecting to IP address: %s", argv[IP_ARG_IDX]);
		industrial::tcp_client::TcpClient robot;
		ros::NodeHandle node;

		robot.init(argv[IP_ARG_IDX], StandardSocketPorts::MOTION);
		motoman::joint_trajectory_downloader::JointTrajectoryDownloader jtHandler(node, &robot);

		ros::spin();
	}
	else
	{
		ROS_ERROR("Missing command line arguments, usage: motion_download_interface <robot ip address>");
	}

  return 0;
}
