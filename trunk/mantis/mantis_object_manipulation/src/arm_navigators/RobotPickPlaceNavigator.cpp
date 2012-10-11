/*
 * RobotPickPlaceNavigator.cpp
 *
 *  Created on: Oct 8, 2012
 *      Author: jnicho
 */

/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2011, Southwest Research Institute
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *       * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *       * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *       * Neither the name of the Southwest Research Institute, nor the names
 *       of its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
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

#include <arm_navigators/RobotPickPlaceNavigator.h>
#include <boost/foreach.hpp>

using namespace trajectory_execution_monitor;

// node name
std::string NODE_NAME = "robot_pick_place_navigation";

// name spaces
std::string MAIN_NAMESPACE = "main";
std::string SEGMENTATION_NAMESPACE = "segmentation";
std::string GOAL_NAMESPACE = "goal";
std::string JOINT_CONFIGURATIONS_NAMESPACE = "joints";

// marker map
std::map<std::string,visualization_msgs::Marker> MarkerMap;
const std::string MARKER_ATTACHED_OBJ = "attached_obj";
const std::string MARKER_SEGMENTED_OBJ = "segmented_obj";


void RobotPickPlaceNavigator::fetchParameters(std::string nameSpace)
{
	ros::NodeHandle nh;
	ros::param::param(nameSpace + "/" + PARAM_NAME_ARM_GROUP,arm_group_name_,DEFAULT_ARM_GROUP);
	ros::param::param(nameSpace + "/" + PARAM_NAME_GRIPPER_GROUP,gripper_group_name_,DEFAULT_GRIPPER_GROUP);
	ros::param::param(nameSpace + "/" + PARAM_NAME_WRIST_LINK,wrist_link_name_,DEFAULT_WRIST_LINK);
	ros::param::param(nameSpace + "/" + PARAM_NAME_GRIPPER_LINK,gripper_link_name_,DEFAULT_GRIPPER_LINK);
	ros::param::param(nameSpace + "/" + PARAM_NAME_GRASP_ACTION_SERVICE,grasp_action_service_,DEFAULT_GRASP_ACTION_SERVICE );
	ros::param::param(nameSpace + "/" + PARAM_NAME_TRAJECTORY_ACTION_SERVICE,trajectory_action_service_,DEFAULT_TRAJECTORY_ACTION_SERVICE);
	ros::param::param(nameSpace + "/" + PARAM_NAME_PATH_PLANNER_SERVICE,path_planner_service_,DEFAULT_PATH_PLANNER_SERVICE);
	ros::param::param(nameSpace + "/" + PARAM_NAME_TRAJECTORY_FILTER_SERVICE,trajectory_filter_service_,DEFAULT_TRAJECTORY_FILTER_SERVICE);
	ros::param::param(nameSpace + "/" + PARAM_NAME_SEGMENTATION_SERVICE,segmentation_service_,DEFAULT_SEGMENTATION_SERVICE);
	ros::param::param(nameSpace + "/" + PARAM_NAME_RECOGNITION_SERVICE,recognition_service_,DEFAULT_RECOGNITION_SERVICE);
	ros::param::param(nameSpace + "/" + PARAM_NAME_GRASP_PLANNING_SERVICE,grasp_planning_service_,DEFAULT_GRASP_PLANNING_SERVICE);
	ros::param::param(nameSpace + "/" + PARAM_NAME_MESH_DATABASE_SERVICE,mesh_database_service_,DEFAULT_MESH_DATABASE_SERVICE);
	ros::param::param(nameSpace + "/" + PARAM_NAME_MODEL_DATABASE_SERVICE,model_database_service_,DEFAULT_MODEL_DATABASE_SERVICE);
	ros::param::param(nameSpace + "/" + PARAM_NAME_PLANNING_SCENE_SERVICE,planning_scene_service_,DEFAULT_ARM_GROUP);
	ros::param::param(nameSpace + "/" + PARAM_NAME_IK_PLUGING,ik_plugin_name_,DEFAULT_IK_PLUGING);
	ros::param::param(nameSpace + "/" + PARAM_NAME_JOINT_STATES_TOPIC,joint_states_topic_,DEFAULT_JOINT_STATES_TOPIC);
}

RobotPickPlaceNavigator::RobotPickPlaceNavigator(ConfigurationFlags flag)
:configuration_type_(flag),
 cm_("robot_description"),
 current_robot_state_(NULL),
 grasp_exec_action_client_(grasp_action_service_,true)
{
	ros::NodeHandle nh;

	// initializing name spaces global strings
	NODE_NAME = ros::this_node::getName();
	MAIN_NAMESPACE = NODE_NAME + "/" + MAIN_NAMESPACE;
	GOAL_NAMESPACE = NODE_NAME + "/" + GOAL_NAMESPACE;
	SEGMENTATION_NAMESPACE = NODE_NAME + "/" + SEGMENTATION_NAMESPACE;
	JOINT_CONFIGURATIONS_NAMESPACE = NODE_NAME + "/" + JOINT_CONFIGURATIONS_NAMESPACE;
}

RobotPickPlaceNavigator::~RobotPickPlaceNavigator()
{
	// TODO Auto-generated destructor stub
}

void RobotPickPlaceNavigator::setup()
{
	ros::NodeHandle nh;
	std::string nodeName = ros::this_node::getName();

	ROS_INFO_STREAM(NODE_NAME<<": Loading ros parameters");

	// getting ros parametets
	fetchParameters(MAIN_NAMESPACE);

	ROS_INFO_STREAM(NODE_NAME<<": Setting up execution Monitors");
	// setting up execution monitors
	{
		joint_state_recorder_.reset(new JointStateTrajectoryRecorder(joint_states_topic_));
		arm_controller_handler_.reset(new FollowJointTrajectoryControllerHandler(arm_group_name_,trajectory_action_service_));
		gripper_controller_handler_.reset(new GraspPoseControllerHandler(gripper_group_name_,grasp_action_service_));

		trajectory_execution_monitor_.addTrajectoryRecorder(joint_state_recorder_);
		trajectory_execution_monitor_.addTrajectoryControllerHandler(arm_controller_handler_);
		trajectory_execution_monitor_.addTrajectoryControllerHandler(gripper_controller_handler_);
	}

	ROS_INFO_STREAM(NODE_NAME<<": Setting up Service Clients");
    // setting up service clients, this is configuration specific
	{
		switch(configuration_type_)
		{
		case SETUP_SPHERE_PICK_PLACE:

			seg_srv_ = nh.serviceClient<tabletop_object_detector::TabletopSegmentation>(segmentation_service_, true);
			break;

		case SETUP_FULL:

			seg_srv_ = nh.serviceClient<tabletop_object_detector::TabletopSegmentation>(segmentation_service_, true);
			rec_srv_ = nh.serviceClient<tabletop_object_detector::TabletopObjectRecognition>(
					recognition_service_, true);
			object_database_model_mesh_client_ = nh.serviceClient<household_objects_database_msgs::GetModelMesh>(
					mesh_database_service_, true);
			object_database_model_description_client_ = nh.serviceClient<household_objects_database_msgs::GetModelDescription>(
					model_database_service_, true);
			break;

		case SETUP_OTHER:

			ROS_ERROR_STREAM(NODE_NAME<<": This configuration option is not supported");
			exit(0);
		}

		grasp_planning_client = nh.serviceClient<object_manipulation_msgs::GraspPlanning>(grasp_planning_service_, true);
		planning_service_client_ = nh.serviceClient<arm_navigation_msgs::GetMotionPlan>(path_planner_service_);

		// arm trajectory filter service
		trajectory_filter_service_client_ = nh.serviceClient<arm_navigation_msgs::FilterJointTrajectoryWithConstraints>(trajectory_filter_service_);
		ROS_INFO_STREAM(NODE_NAME<<": Waiting for trajectory filter service");
		trajectory_filter_service_client_.waitForExistence();
		ROS_INFO_STREAM(NODE_NAME<<": Trajectory filter service connected");

		// planing scene
		ROS_INFO_STREAM(NODE_NAME <<": Waiting for " + planning_scene_service_ + " service");
		ros::service::waitForService(planning_scene_service_);
		set_planning_scene_diff_client_ = nh.serviceClient<arm_navigation_msgs::SetPlanningSceneDiff>(planning_scene_service_);
	}

	// will use grasp execution client to request pre-grasp action since the default gripper controller handler
	// ignores this step.
	ROS_INFO_STREAM(NODE_NAME << ": Setting up Service Action Clients");
	{
		while(!grasp_exec_action_client_.waitForServer(ros::Duration(0.5)))
		{
			ROS_INFO_STREAM(NODE_NAME << "Waiting for action service "<< grasp_action_service_);
		}
		ROS_INFO_STREAM(NODE_NAME<<" : Connected to action service "<<grasp_action_service_);
	}

	ROS_INFO_STREAM(NODE_NAME<<": Setting up ros publishers");
	{
		// setting up ros publishers
		vis_marker_publisher_ = nh.advertise<visualization_msgs::Marker> ("freetail_object", 1);
		vis_marker_array_publisher_ = nh.advertise<visualization_msgs::MarkerArray> ("freetail_object_array", 1);
		attached_object_publisher_ = nh.advertise<arm_navigation_msgs::AttachedCollisionObject> ("attached_collision_object_alternate", 1);

		// setting up timer obj
		_MarkerPubTimer = nh.createTimer(ros::Duration(0.4f),&RobotPickPlaceNavigator::callbackPublishMarkers,this);

		ROS_INFO_STREAM(NODE_NAME<<": Setting up dynamic libraries");
		// others
		grasp_tester_ = new object_manipulator::GraspTesterFast(&cm_, ik_plugin_name_);
		place_tester_ = new PlaceSequenceValidator(&cm_, ik_plugin_name_);
		trajectories_finished_function_ = boost::bind(&RobotPickPlaceNavigator::trajectoriesFinishedCallbackFunction, this, _1);

		ROS_INFO_STREAM(NODE_NAME<<": Finished setup");
	}

	ROS_INFO_STREAM(NODE_NAME<<": Setting up published markers");
	{
		visualization_msgs::Marker marker;
		marker.header.frame_id = cm_.getWorldFrameId();
		marker.ns = NODE_NAME;
		marker.type = visualization_msgs::Marker::SPHERE;
		marker.action = visualization_msgs::Marker::DELETE;
		tf::poseTFToMsg(tf::Transform::getIdentity(),marker.pose);

		// adding marker to map
		marker.id = 0; MarkerMap.insert(std::make_pair(MARKER_SEGMENTED_OBJ,marker));
		marker.id = 1; MarkerMap.insert(std::make_pair(MARKER_ATTACHED_OBJ,marker));
	}
}

void RobotPickPlaceNavigator::setupBallPickingDemo()
{
	ros::NodeHandle nh;
	std::string nodeName = ros::this_node::getName();

	ROS_INFO("Loading ros parameters");

	// getting ros parametets
	fetchParameters(MAIN_NAMESPACE);

	ROS_INFO_STREAM(NODE_NAME<<": Setting up execution Monitors");
	// setting up execution monitors
	{
		joint_state_recorder_.reset(new JointStateTrajectoryRecorder(joint_states_topic_));
		arm_controller_handler_.reset(new FollowJointTrajectoryControllerHandler(arm_group_name_,trajectory_action_service_));
		gripper_controller_handler_.reset(new GraspPoseControllerHandler(gripper_group_name_,grasp_action_service_));

		trajectory_execution_monitor_.addTrajectoryRecorder(joint_state_recorder_);
		trajectory_execution_monitor_.addTrajectoryControllerHandler(arm_controller_handler_);
		trajectory_execution_monitor_.addTrajectoryControllerHandler(gripper_controller_handler_);
	}

	ROS_INFO_STREAM(NODE_NAME<<": Setting up Service Clients");
    // setting up service clients
	{
		// segmentation service
		seg_srv_ = nh.serviceClient<tabletop_object_detector::TabletopSegmentation>(segmentation_service_, true);

		// grasp planning
		grasp_planning_client = nh.serviceClient<object_manipulation_msgs::GraspPlanning>(grasp_planning_service_, true);

		// arm path planning service
		planning_service_client_ = nh.serviceClient<arm_navigation_msgs::GetMotionPlan>(path_planner_service_);

		// arm trajectory filter service
		trajectory_filter_service_client_ = nh.serviceClient<arm_navigation_msgs::FilterJointTrajectoryWithConstraints>(trajectory_filter_service_);
		ROS_INFO_STREAM(NODE_NAME<<": Waiting for trajectory filter service");
		trajectory_filter_service_client_.waitForExistence();
		ROS_INFO_STREAM(NODE_NAME<<": Trajectory filter service connected");

		// planing scene
		ROS_INFO_STREAM(NODE_NAME <<": Waiting for " + planning_scene_service_ + " service");
		ros::service::waitForService(planning_scene_service_);
		set_planning_scene_diff_client_ = nh.serviceClient<arm_navigation_msgs::SetPlanningSceneDiff>(planning_scene_service_);
	}

	// will use grasp execution client to request pre-grasp action since the default gripper controller handler
	// ignores this step.
	ROS_INFO_STREAM(NODE_NAME << ": Setting up Service Action Clients");
	{
		//grasp_exec_action_client_ = actionlib::SimpleActionClient<object_manipulation_msgs::GraspHandPostureExecutionAction>(grasp_action_service,true);
		while(!grasp_exec_action_client_.waitForServer(ros::Duration(0.5)))
		{
			ROS_INFO_STREAM(NODE_NAME << "Waiting for action service "<< grasp_action_service_);
		}
		ROS_INFO_STREAM(NODE_NAME<<" : Connected to action service "<<grasp_action_service_);
	}

	ROS_INFO_STREAM(NODE_NAME<<": Setting up ros publishers");
	{
		// setting up ros publishers
		vis_marker_publisher_ = nh.advertise<visualization_msgs::Marker> ("freetail_object", 1);
		vis_marker_array_publisher_ = nh.advertise<visualization_msgs::MarkerArray> ("freetail_object_array", 1);
		attached_object_publisher_ = nh.advertise<arm_navigation_msgs::AttachedCollisionObject> ("attached_collision_object_alternate", 1);

		// setting up timer obj
		_MarkerPubTimer = nh.createTimer(ros::Duration(0.4f),&RobotPickPlaceNavigator::callbackPublishMarkers,this);

		ROS_INFO_STREAM(NODE_NAME<<": Setting up dynamic libraries");
		// others
		grasp_tester_ = new object_manipulator::GraspTesterFast(&cm_, ik_plugin_name_);
		place_tester_ = new PlaceSequenceValidator(&cm_, ik_plugin_name_);
		trajectories_finished_function_ = boost::bind(&RobotPickPlaceNavigator::trajectoriesFinishedCallbackFunction, this, _1);

		ROS_INFO_STREAM(NODE_NAME<<": Finished setup");
	}

	ROS_INFO_STREAM(NODE_NAME<<": Setting up published markers");
	{
		visualization_msgs::Marker marker;
		marker.header.frame_id = cm_.getWorldFrameId();
		marker.ns = NODE_NAME;
		marker.type = visualization_msgs::Marker::SPHERE;
		marker.action = visualization_msgs::Marker::DELETE;
		tf::poseTFToMsg(tf::Transform::getIdentity(),marker.pose);

		// adding marker to map
		marker.id = 0; MarkerMap.insert(std::make_pair(MARKER_SEGMENTED_OBJ,marker));
		marker.id = 1; MarkerMap.insert(std::make_pair(MARKER_ATTACHED_OBJ,marker));
	}
}

void RobotPickPlaceNavigator::setupRecognitionOnly()
{
	ros::NodeHandle nh;

	ROS_INFO("Loading ros parameters");

	// getting ros parametets
	fetchParameters(MAIN_NAMESPACE);

	ROS_INFO("Setting up Service Clients");
    // setting up service clients
	{
		rec_srv_ = nh.serviceClient<tabletop_object_detector::TabletopObjectRecognition>(recognition_service_, true);

		ROS_INFO_STREAM("Waiting for " + recognition_service_+ " service");
		while(!ros::service::waitForService(recognition_service_,ros::Duration(4.0f)))
		{
			ROS_INFO_STREAM("Waiting for " + recognition_service_+ " service");
		}
		ROS_INFO_STREAM("Connected to " + recognition_service_+ " service");
	}
}

bool RobotPickPlaceNavigator::trajectoriesFinishedCallbackFunction(TrajectoryExecutionDataVector tedv)
{
	if(tedv.size()==0)
	{
		ROS_ERROR_STREAM(NODE_NAME <<": trajectory finished callback received empty vector");
		return true;
	}

   last_trajectory_execution_data_vector_ = tedv;

   trajectories_succeeded_ = (tedv.back().result_ == SUCCEEDED
                              || tedv.back().result_ == HANDLER_REPORTS_FAILURE_BUT_OK
                              || tedv.back().result_ == HANDLER_REPORTS_FAILURE_BUT_CLOSE_ENOUGH);

   ROS_INFO_STREAM(NODE_NAME<<": Trajectories " << tedv.size() << " ok " << trajectories_succeeded_);

   for(unsigned int i = 0; i < tedv.size(); i++)
   {
     ROS_INFO_STREAM("Recorded trajectory " << i << " has " << tedv[i].recorded_trajectory_.points.size());
   }
   execution_completed_.notify_all();
   return true;
 }

void RobotPickPlaceNavigator::revertPlanningScene()
{

  if(current_robot_state_ != NULL)
  {
    cm_.revertPlanningScene(current_robot_state_);
    current_robot_state_ = NULL;
  }
  last_mpr_id_ = 0;
  max_mpr_id_ = 0;
  max_trajectory_id_ = 0;
}

std::vector<std::string> RobotPickPlaceNavigator::getJointNames(const std::string& group) {
  if(cm_.getKinematicModel()->getModelGroup(group) == NULL) {
    std::vector<std::string> names;
    return names;
  }
  return cm_.getKinematicModel()->getModelGroup(group)->getJointModelNames();
}

void RobotPickPlaceNavigator::updateCurrentJointStateToLastTrajectoryPoint(const trajectory_msgs::JointTrajectory& traj)
{
  if(traj.points.size() == 0)
  {
    ROS_ERROR_STREAM(NODE_NAME<<": No points in trajectory for setting current state");
    return;
  }

  std::map<std::string, double> joint_vals;
  for(unsigned int i = 0; i < traj.joint_names.size(); i++)
  {
    joint_vals[traj.joint_names[i]] = traj.points.back().positions[i];
  }

  current_robot_state_->setKinematicState(joint_vals);
}

bool RobotPickPlaceNavigator::getAndSetPlanningScene()
{
  ros::WallTime start_time = ros::WallTime::now();
  arm_navigation_msgs::SetPlanningSceneDiff::Request planning_scene_req;
  arm_navigation_msgs::SetPlanningSceneDiff::Response planning_scene_res;

  revertPlanningScene();

  planning_scene_req.planning_scene_diff = planning_scene_diff_;

  ROS_DEBUG_STREAM(NODE_NAME<<": Getting and setting planning scene");

  if(!set_planning_scene_diff_client_.call(planning_scene_req, planning_scene_res))
  {
    ROS_WARN_STREAM(NODE_NAME<<": Can't get planning scene");
    return false;
  }

  current_planning_scene_ = planning_scene_res.planning_scene;
  current_robot_state_ = cm_.setPlanningScene(current_planning_scene_);

  if(current_robot_state_ == NULL)
  {
    ROS_WARN_STREAM(NODE_NAME<<": Problems setting robot kinematic state");
    return false;
  }

  // Change time stamp to avoid saving sim time.
  current_planning_scene_.robot_state.joint_state.header.stamp = ros::Time(ros::WallTime::now().toSec());
  ROS_INFO_STREAM(NODE_NAME<<": Setting took " << (ros::WallTime::now()-start_time).toSec());
  planning_scene_duration_ += ros::WallTime::now()-start_time;
  return true;
}

bool RobotPickPlaceNavigator::moveArm(const std::string& group_name,const std::vector<double>& joint_positions)
{

  // requesting path plan
  ros::WallTime start_time = ros::WallTime::now();
  arm_navigation_msgs::GetMotionPlan::Request plan_req;
  arm_navigation_msgs::GetMotionPlan::Response plan_res;

  plan_req.motion_plan_request.allowed_planning_time = ros::Duration(5.0);
  plan_req.motion_plan_request.group_name = group_name;

  planning_environment::convertKinematicStateToRobotState(*current_robot_state_,
                                                          ros::Time::now(),
                                                          cm_.getWorldFrameId(),
                                                          plan_req.motion_plan_request.start_state);

  std::vector<std::string> joint_names = getJointNames(group_name);
  std::vector<arm_navigation_msgs::JointConstraint>& joint_constraints =  plan_req.motion_plan_request.goal_constraints.joint_constraints;

  joint_constraints.resize(joint_names.size());
  for(unsigned int i = 0; i < joint_names.size(); i++)
  {
    joint_constraints[i].joint_name = joint_names[i];
    joint_constraints[i].position = joint_positions[i];
    joint_constraints[i].tolerance_above = .01;
    joint_constraints[i].tolerance_below = .01;
  }

  if(!planning_service_client_.call(plan_req, plan_res))
  {
    ROS_WARN_STREAM(NODE_NAME<<": Planner service call failed");
    return false;
  }

  if(plan_res.error_code.val != plan_res.error_code.SUCCESS)
  {
    ROS_WARN_STREAM(NODE_NAME<<": Planner failed");
    return false;
  }

  motion_planning_duration_ += ros::WallTime::now()-start_time;
  start_time = ros::WallTime::now();

  last_mpr_id_ = max_mpr_id_;
  max_mpr_id_++;

  // requesting planned trajectory filtering
  arm_navigation_msgs::FilterJointTrajectoryWithConstraints::Request filter_req;
  arm_navigation_msgs::FilterJointTrajectoryWithConstraints::Response filter_res;

  filter_req.trajectory = plan_res.trajectory.joint_trajectory;
  filter_req.start_state = plan_req.motion_plan_request.start_state;
  filter_req.group_name = group_name;
  filter_req.goal_constraints = plan_req.motion_plan_request.goal_constraints;
  filter_req.allowed_time = ros::Duration(4.0);

  if(!trajectory_filter_service_client_.call(filter_req, filter_res)) {
    ROS_WARN_STREAM(NODE_NAME<<": Filter service call failed");
    return false;
  }

  if(filter_res.error_code.val != filter_res.error_code.SUCCESS) {
    ROS_WARN_STREAM(NODE_NAME<<": Filter failed");
    return false;
  }
  trajectory_filtering_duration_ += ros::WallTime::now()-start_time;

  // requesting trajectory execution action
  trajectories_succeeded_ = false;
  TrajectoryExecutionRequest ter;
  ter.group_name_ = group_name;
  ter.controller_name_ = arm_controller_handler_->getControllerName();
  ter.trajectory_ = filter_res.trajectory;
  ter.test_for_close_enough_ = false;
  ter.failure_time_factor_ = 1000;
  ter.max_joint_distance_ = .01;

  std::vector<TrajectoryExecutionRequest> ter_reqs;
  ter_reqs.push_back(ter);

  ros::WallTime start_execution = ros::WallTime::now();

  ROS_INFO_STREAM(NODE_NAME<<": Should be trying to move arm");
  trajectory_execution_monitor_.executeTrajectories(ter_reqs,trajectories_finished_function_);

  boost::unique_lock<boost::mutex> lock(execution_mutex_);
  execution_completed_.wait(lock);
  execution_duration_ += (ros::WallTime::now()-start_execution);

  arm_navigation_msgs::ArmNavigationErrorCodes error_code;
  if(trajectories_succeeded_)
  {
    error_code.val = error_code.SUCCESS;
    ROS_INFO_STREAM(NODE_NAME<<": Trajectory execution has succeeded");
  }
  else
  {
    error_code.val = error_code.PLANNING_FAILED;
    ROS_INFO_STREAM(NODE_NAME<<": Trajectory execution has failed");
  }

  return trajectories_succeeded_;
}

bool RobotPickPlaceNavigator::validateJointTrajectory(trajectory_msgs::JointTrajectory &jt)
{
	for(unsigned int i = 0; i < jt.points.size(); i++)
	{
		trajectory_msgs::JointTrajectoryPoint &p = jt.points[i];
		if(p.positions.size() == 0)
		{
			return false; // should at least have position values
		}

		if(p.velocities.size() == 0)
		{
			p.velocities = std::vector<double>(jt.joint_names.size(),0.0f);
		}

		if(p.accelerations.size() == 0)
		{
			p.accelerations = std::vector<double>(jt.joint_names.size(),0.0f);
		}
	}

	return true;
}

void RobotPickPlaceNavigator::printJointTrajectory(const trajectory_msgs::JointTrajectory &jt)
{

	ROS_INFO_STREAM(NODE_NAME<<": Total joint points: "<<jt.points.size()<<", total joints "<<jt.joint_names.size());
	std::stringstream ss;
	for(unsigned int j = 0; j < jt.points.size(); j++)
	{
		ROS_INFO_STREAM(NODE_NAME<<": joint point time: "<<jt.points[j].time_from_start.toSec());
		for(unsigned int i = 0;i < jt.joint_names.size(); i++)
		{
			ss<<NODE_NAME<<": joint "<<jt.joint_names[i]<<", pos: ";
			if(jt.points[j].positions.size() == 0)
			{
				ss<<"NAN";
			}
			else
			{
				ss<<jt.points[j].positions[i];
			}

			ss<<", vel: ";
			if(jt.points[j].velocities.size() == 0)
			{
				ss<<"NAN";
			}
			else
			{
				ss<<jt.points[j].velocities[i];
			}

			ss<<", acc: ";
			if(jt.points[j].velocities.size() == 0)
			{
				ss<<"NAN";
			}
			else
			{
				ss<<jt.points[j].accelerations[i];
			}

			ROS_INFO_STREAM(ss.str());
			ss.str("");
		}
	}
}

bool RobotPickPlaceNavigator::fastFilterTrajectory(const std::string& group_name,trajectory_msgs::JointTrajectory& jt)
{
  arm_navigation_msgs::FilterJointTrajectoryWithConstraints::Request filter_req;
  arm_navigation_msgs::FilterJointTrajectoryWithConstraints::Response filter_res;

  ROS_INFO_STREAM(NODE_NAME<<": Performing fast filter trajectory");
  ROS_INFO_STREAM(NODE_NAME<<": Incoming number of joints: " << jt.joint_names.size());
  ROS_INFO_STREAM(NODE_NAME<<": Incoming number of points: " << jt.points.size());

  std::map<std::string, double> jvals;
  for(unsigned int i = 0; i < jt.joint_names.size(); i++)
  {
    jvals[jt.joint_names[i]] = jt.points[0].positions[i];

    ROS_INFO_STREAM(NODE_NAME<<": Populating joint names: " << jt.joint_names[i]
                    << ", jvals: " << jvals[jt.joint_names[i]]
                    << ", jt.points: " << jt.points[0].positions[i]);
  }

  planning_models::KinematicState state(*current_robot_state_);
  state.setKinematicState(jvals);

  filter_req.trajectory = jt;
  planning_environment::convertKinematicStateToRobotState(state,
                                                          ros::Time::now(),
                                                          cm_.getWorldFrameId(),
                                                          filter_req.start_state);
  filter_req.group_name = group_name;

  if(!trajectory_filter_service_client_.call(filter_req, filter_res))
  {
    ROS_WARN_STREAM(NODE_NAME<<": Fast Filter service call failed");
    ROS_WARN_STREAM(NODE_NAME<<": Return true anyway");
    return true;
  }

  if (filter_res.trajectory.points.size() != 0)
  {
      jt = filter_res.trajectory;
  }
  else
  {
      ROS_WARN_STREAM(NODE_NAME<<": Filter returned an empty trajectory (ignoring for now)");
  }

  ROS_INFO_STREAM(NODE_NAME<<": Filtered number of joints: " << jt.joint_names.size());
  ROS_INFO_STREAM(NODE_NAME<<": Filtered number of points: " << jt.points.size());
  return true;
}

bool RobotPickPlaceNavigator::moveArmToSide()
{

    getAndSetPlanningScene();

    //name: ['joint_s', 'joint_l', 'joint_e', 'joint_u', 'joint_r', 'joint_b', 'joint_t']
    //position: [-0.46665400266647339, 0.1069866344332695, 2.1171059608459473, -1.4666222333908081, -0.17949093878269196, -1.6554385423660278, -1.7431133985519409]

    //-3.0410828590393066, 0.10696959495544434, 2.117098093032837, -1.466621994972229, -0.17949093878269196, -1.6554234027862549, -1.7430833578109741

    //position: [0.16656530392591254, 0.46065822721369176, 2.4644586717834467, 0.49449136755439443, -0.2900361153401066, 1.4113548618662812, 2.3286899342716625]


//    std::vector<double> joint_angles;
//    joint_angles.push_back(-1.0410828590393066);
//    joint_angles.push_back(0.46065822721369176);
//    joint_angles.push_back(2.4644586717834467);
//    joint_angles.push_back(0.49449136755439443);
//    joint_angles.push_back(-0.2900361153401066);
//    joint_angles.push_back(1.4113548618662812);
//    joint_angles.push_back(2.3286899342716625);
    _JointConfigurations.fetchParameters(JOINT_CONFIGURATIONS_NAMESPACE);
    return moveArm(arm_group_name_,_JointConfigurations.SideAngles);
}

void RobotPickPlaceNavigator::addDetectedTableToPlanningSceneDiff(const tabletop_object_detector::Table &table)
{
  arm_navigation_msgs::CollisionObject table_object;
  table_object.operation.operation = arm_navigation_msgs::CollisionObjectOperation::ADD;
  table_object.shapes.resize(1);
  table_object.shapes[0].type = arm_navigation_msgs::Shape::BOX;
  table_object.shapes[0].dimensions.resize(3);
  table_object.shapes[0].dimensions[0] = fabs(table.x_max-table.x_min);
  table_object.shapes[0].dimensions[1] = fabs(table.y_max-table.y_min);
  table_object.shapes[0].dimensions[2] = 0.01;
  table_object.id = "table";

  // finding table transform in world coordinates
  geometry_msgs::PoseStamped table_world;
  cm_.convertPoseGivenWorldTransform(*current_robot_state_,
                                     cm_.getWorldFrameId(),
                                     table.pose.header,
                                     table.pose.pose,
                                     table_world);

  table_object.header = table_world.header;

  //set the origin of the table object in the middle of the table
  tf::Transform table_trans;
  tf::poseMsgToTF(table_world.pose, table_trans);
  tf::Transform table_translation;
  table_translation.setIdentity();
  table_translation.setOrigin( tf::Vector3( (table.x_min + table.x_max)/2.0, (table.y_min + table.y_max)/2.0, 0.0) );
  table_trans = table_trans * table_translation;
  table_object.poses.resize(1);
  tf::poseTFToMsg(table_trans, table_object.poses[0]);


  ROS_INFO_STREAM(NODE_NAME<<": Table location is " << table_object.poses[0].position.x << " "
                  << table_object.poses[0].position.y << " "
                  << table_object.poses[0].position.z);

  // saving results from table object
  planning_scene_diff_.collision_objects.push_back(table_object);
  table_ = table_object;
}

void RobotPickPlaceNavigator::addDetectedObjectToPlanningSceneDiff(arm_navigation_msgs::CollisionObject &obj)
{
  obj.operation.operation = arm_navigation_msgs::CollisionObjectOperation::ADD;
  planning_scene_diff_.collision_objects.push_back(obj);
}

void RobotPickPlaceNavigator::addDetectedObjectToPlanningSceneDiff(const household_objects_database_msgs::DatabaseModelPoseList& model)
{
  arm_navigation_msgs::CollisionObject obj;
  obj.id = makeCollisionObjectNameFromModelId(model.model_list[0].model_id);

  // finding pose of object in world coordinates
  geometry_msgs::PoseStamped obj_world;
  cm_.convertPoseGivenWorldTransform(*current_robot_state_,
                                     cm_.getWorldFrameId(),
                                     model.model_list[0].pose.header,
                                     model.model_list[0].pose.pose,
                                     obj_world);

  obj.operation.operation = arm_navigation_msgs::CollisionObjectOperation::ADD;
  getMeshFromDatabasePose(model.model_list[0],
                          obj,
                          obj_world);

  ROS_INFO_STREAM(NODE_NAME<<": Pushing back " << obj.id << " frame " << obj.header.frame_id);
  planning_scene_diff_.collision_objects.push_back(obj);
}

bool RobotPickPlaceNavigator::performSegmentation()
{
	//  ===================================== saving current time stamp =====================================
	ros::WallTime start  = ros::WallTime::now();

	//  ===================================== clearing results from last call =====================================
	planning_scene_diff_.collision_objects.clear();
	recognized_obj_pose_map_.clear();

	//  ===================================== calling service =====================================
	tabletop_object_detector::TabletopSegmentation segmentation_srv;
	bool success = seg_srv_.call(segmentation_srv);

	// printing timing
	ros::WallTime after_seg = ros::WallTime::now();
	ROS_INFO_STREAM(NODE_NAME<<": Seg took " << (after_seg-start).toSec());

	// ===================================== checking results ========================================
	if (!success)
	{
		ROS_ERROR_STREAM(NODE_NAME<<": Call to segmentation service failed");
		return false;
	}

	if (segmentation_srv.response.result != segmentation_srv.response.SUCCESS)
	{
		ROS_ERROR_STREAM(NODE_NAME<<": Segmentation service returned error "<< segmentation_srv.response.result);
		return false;
	}

	if(segmentation_srv.response.clusters.size() == 0)
	{
		ROS_ERROR_STREAM(NODE_NAME<<": Tabletop segmentation returned 0 clusters, exiting");
		return false;
	}

	//  ===================================== storing results =====================================
	segmentation_results_ = segmentation_srv.response;
	segmented_clusters_ = segmentation_srv.response.clusters;

	//  ===================================== updating local planning scene =====================================
	addDetectedTableToPlanningSceneDiff(segmentation_srv.response.table);

	// ===================================== printing completion info message =====================================
	ROS_INFO_STREAM(NODE_NAME<<": Segmentation service succeeded. Detected "<<(int)segmentation_srv.response.clusters.size()<<" clusters");

	return true;
}



bool RobotPickPlaceNavigator::performSphereSegmentation()
{
	//  ===================================== preparing result objects =====================================
	  arm_navigation_msgs::CollisionObject obj;
	  int bestClusterIndex = -1;
	  sensor_msgs::PointCloud sphereCluster;

	  //  ===================================== calling segmentation =====================================
	  _SphereSeg.fetchParameters(SEGMENTATION_NAMESPACE);
	  bool success = _SphereSeg.segment(segmented_clusters_,obj,bestClusterIndex);

	  // ===================================== checking results ========================================
	  if(!success)
	  {
		  ROS_ERROR_STREAM(NODE_NAME<<": Sphere segmentation did not succeed");
		  return false;
	  }

	  //  ===================================== storing results =====================================
	  // assigning id first
	  obj.id = makeCollisionObjectNameFromModelId(0);

	  // retrieving segmented sphere cluster
	  _SphereSeg.getSphereCluster(sphereCluster);

	  // storing best cluster
	  segmented_clusters_.clear();
	  segmented_clusters_.push_back(sphereCluster);

	  // storing pose
	  geometry_msgs::PoseStamped pose;
	  pose.header.frame_id = obj.header.frame_id;
	  pose.pose = obj.poses[0];
	  recognized_obj_pose_map_[std::string(obj.id)] = pose;

	  // storing recognized object as model for planning
	  household_objects_database_msgs::DatabaseModelPoseList models;
	  household_objects_database_msgs::DatabaseModelPose model;
	  model.model_id = 0;
	  model.pose = pose;
	  model.confidence = 1.0f;
	  model.detector_name = "sphere_segmentation";
	  models.model_list.push_back(model);
	  recognized_models_.clear();
	  recognized_models_.push_back(models);

	  //  ===================================== updating local planning scene =====================================
	  addDetectedObjectToPlanningSceneDiff(obj);

	  //  ===================================== updating markers =====================================
	  // update markers
	  visualization_msgs::Marker &segMarker = MarkerMap[MARKER_SEGMENTED_OBJ];
	  visualization_msgs::Marker &attachedMarker = MarkerMap[MARKER_ATTACHED_OBJ];

	  // segmented object
	  collisionObjToMarker(obj,segMarker);
	  segMarker.action = visualization_msgs::Marker::ADD;

	  // attached object, hide for now until gripper is close
	  collisionObjToMarker(obj,attachedMarker);
	  attachedMarker.action = visualization_msgs::Marker::DELETE;
	  attachedMarker.header.frame_id = gripper_link_name_;

	  // ===================================== printing completion info message =====================================
	  arm_navigation_msgs::Shape &shape = obj.shapes[0];
	  ROS_INFO_STREAM("\n"<<NODE_NAME<<": Sphere Segmentation completed:\n");
	  ROS_INFO_STREAM("\tFrame id: "<<obj.header.frame_id<<"\n");
	  ROS_INFO_STREAM("\tRadius: "<<shape.dimensions[0]<<"\n");
	  ROS_INFO_STREAM("\tx: "<<pose.pose.position.x<<", y: "<<pose.pose.position.y
			  <<", z: "<<pose.pose.position.z<<"\n");

	return true;
}

bool RobotPickPlaceNavigator::performRecognition()
{
	//  ===================================== saving current time stamp =====================================
	ros::WallTime start = ros::WallTime::now();

	//  ===================================== clearing results from last call =====================================
	planning_scene_diff_.collision_objects.clear();
	recognized_obj_pose_map_.clear();

	//  ===================================== calling service =====================================
	tabletop_object_detector::TabletopObjectRecognition recognition_srv;
	recognition_srv.request.table = segmentation_results_.table;
	recognition_srv.request.clusters = segmented_clusters_;
	recognition_srv.request.num_models = 1;
	recognition_srv.request.perform_fit_merge = false;

	bool success = rec_srv_.call(recognition_srv);

	// ===================================== checking results ========================================
	if (!success)
	{
		ROS_ERROR_STREAM(NODE_NAME<<": Call to recognition service " <<recognition_service_.c_str()<<" failed.");
		return false;
	}

    if(recognition_srv.response.models.size() == 0)
    {
    	ROS_ERROR_STREAM(NODE_NAME<<": Recognition found no objects");
    	return false;
    }

	//  ===================================== storing results =====================================
    recognized_models_ = recognition_srv.response.models;

	//  ===================================== updating local planning scene =====================================
    addDetectedObjectToPlanningSceneDiff(recognition_srv.response.models[0]);

	// ===================================== printing completion info message =====================================
    ROS_INFO_STREAM(NODE_NAME<<": Recognition took " << (ros::WallTime::now()-start));
    ROS_INFO_STREAM(NODE_NAME<<": Got " << recognition_srv.response.models.size() << " models");
    recognized_models_ = recognition_srv.response.models;
	std::stringstream stdout;
	stdout<<"\nRetrieved models:";
    BOOST_FOREACH(household_objects_database_msgs::DatabaseModelPose model,recognition_srv.response.models[0].model_list)
    {
    	stdout<<"\n\tModel id: "<<model.model_id;
    	stdout<<"\n\tPose frame id: "<<model.pose.header.frame_id;
    	stdout<<"\n\tdetector name: "<<model.detector_name;
    	stdout<<"\n";
    }
    ROS_INFO_STREAM(stdout.str());

	return true;
}

bool RobotPickPlaceNavigator::segmentSpheres()
{
	  // starting timer
	  ros::WallTime start = ros::WallTime::now();

	  // clearing arrays
	  planning_scene_diff_.collision_objects.clear();
	  recognized_obj_pose_map_.clear();


	  tabletop_object_detector::TabletopSegmentation segmentation_srv;
	  if (!seg_srv_.call(segmentation_srv))
	  {
	    ROS_ERROR_STREAM(NODE_NAME<<": Call to segmentation service failed");
	    return false;
	  }

	  // checking result
	  ros::WallTime after_seg = ros::WallTime::now();
	  ROS_INFO_STREAM(NODE_NAME<<": Seg took " << (after_seg-start).toSec());
	  if (segmentation_srv.response.result != segmentation_srv.response.SUCCESS)
	  {
	    ROS_ERROR_STREAM(NODE_NAME<<": Segmentation service returned error "<< segmentation_srv.response.result);
	    return false;
	  }

	  // adding table to environment
	  addDetectedTableToPlanningSceneDiff(segmentation_srv.response.table);
	  ROS_INFO_STREAM(NODE_NAME<<": Segmentation service succeeded. Detected " <<(int)segmentation_srv.response.clusters.size()<<" clusters.");

	  // store segmented clusters for planning
	  if(segmentation_srv.response.clusters.size() > 0)
	  {
		  ROS_INFO_STREAM(NODE_NAME<<": Tabletop segmentation found "<<segmentation_srv.response.clusters.size()<<" clusters");
		  //segmented_clusters_ = segmentation_srv.response.clusters;
	  }
	  else
	  {
		  ROS_ERROR_STREAM(NODE_NAME<<": Tabletop segmentation returned 0 clusters");
		  return false;
	  }

	  // sphere segmentation
	  arm_navigation_msgs::CollisionObject obj;
	  int bestClusterIndex = -1;
	  _SphereSeg.fetchParameters(SEGMENTATION_NAMESPACE);
	  sensor_msgs::PointCloud sphereCluster;
	  if(_SphereSeg.segment(segmentation_srv.response.clusters,obj,bestClusterIndex))
	  {
		  // retrieving segmented sphere cluster
		  _SphereSeg.getSphereCluster(sphereCluster);

		  // storing best cluster
		  segmented_clusters_.clear();
		  segmented_clusters_.push_back(sphereCluster);
		  //segmented_clusters_.push_back(segmentation_srv.response.clusters[bestClusterIndex]);

		  arm_navigation_msgs::Shape &shape = obj.shapes[0];
		  geometry_msgs::Pose &pose = obj.poses[0];

		  ROS_INFO_STREAM("\n"<<NODE_NAME<<": Sphere found:\n");
		  ROS_INFO_STREAM("\tFrame id: "<<obj.header.frame_id<<"\n");
		  ROS_INFO_STREAM("\tRadius: "<<shape.dimensions[0]<<"\n");
		  ROS_INFO_STREAM("\tx: "<<pose.position.x<<", y: "<<pose.position.y<<", z: "<<pose.position.z<<"\n");
	  }
	  else
	  {
		  ROS_ERROR_STREAM(NODE_NAME<<": Sphere segmentation did not succeed");
		  return false;
	  }

	  // reassigning id
	  obj.id = makeCollisionObjectNameFromModelId(0);

	  // adding to planning scene
	  addDetectedObjectToPlanningSceneDiff(obj);

	  // storing pose
	  geometry_msgs::PoseStamped pose;
	  pose.header.frame_id = obj.header.frame_id;
	  pose.pose = obj.poses[0];
	  recognized_obj_pose_map_[std::string(obj.id)] = pose;

	  // creating object array for planning
	  household_objects_database_msgs::DatabaseModelPoseList models;
	  household_objects_database_msgs::DatabaseModelPose model;
	  model.model_id = 0;
	  model.pose = pose;
	  model.confidence = 1.0f;
	  model.detector_name = "sphere_segmentation";
	  models.model_list.push_back(model);

	  // storing model for planning
	  recognized_models_.clear();
	  recognized_models_.push_back(models);

	  // storing final total time
	  perception_duration_ += ros::WallTime::now()-start;

	  // update markers
	  visualization_msgs::Marker &segMarker = MarkerMap[MARKER_SEGMENTED_OBJ];
	  visualization_msgs::Marker &attachedMarker = MarkerMap[MARKER_ATTACHED_OBJ];

	  // segmented object
	  collisionObjToMarker(obj,segMarker);
	  segMarker.action = visualization_msgs::Marker::ADD;
	  //MarkerMap[MARKER_SEGMENTED_OBJ] = segMarker;

	  // attached object, hide for now until gripper is close
	  collisionObjToMarker(obj,attachedMarker);
	  attachedMarker.action = visualization_msgs::Marker::DELETE;
	  attachedMarker.header.frame_id = gripper_link_name_;
	  //MarkerMap[MARKER_ATTACHED_OBJ] = attachedMarker;

	return true;
}

bool RobotPickPlaceNavigator::segmentAndRecognize()
{
  // tabletop segmentation
  ros::WallTime start = ros::WallTime::now();
  planning_scene_diff_.collision_objects.clear();
  recognized_obj_pose_map_.clear();
  tabletop_object_detector::TabletopSegmentation segmentation_srv;
  if (!seg_srv_.call(segmentation_srv))
  {
    ROS_ERROR_STREAM(NODE_NAME<<": Call to segmentation service failed");
    return false;
  }

  ros::WallTime after_seg = ros::WallTime::now();
  ROS_INFO_STREAM("Seg took " << (after_seg-start).toSec());
  if (segmentation_srv.response.result != segmentation_srv.response.SUCCESS)
  {
    ROS_ERROR_STREAM(NODE_NAME<<": Segmentation service returned error "<< segmentation_srv.response.result);
    return false;
  }

  addDetectedTableToPlanningSceneDiff(segmentation_srv.response.table);
  ROS_INFO_STREAM(NODE_NAME<<": Segmentation service succeeded. Detected "
		  <<(int)segmentation_srv.response.clusters.size()<<" clusters");

  // proceed with recognition
  bool got_recognition = false;
  recognized_models_.clear();
  if(!segmentation_srv.response.clusters.empty())
  {
    segmented_clusters_ = segmentation_srv.response.clusters;
    tabletop_object_detector::TabletopObjectRecognition recognition_srv;
    recognition_srv.request.table = segmentation_srv.response.table;
    recognition_srv.request.clusters = segmentation_srv.response.clusters;
    recognition_srv.request.num_models = 1;
    recognition_srv.request.perform_fit_merge = false;

    if (!rec_srv_.call(recognition_srv))
    {
      ROS_ERROR_STREAM(NODE_NAME<<": Call to recognition service failed");
      return false;
    }

    // tabletop recognition
    if(recognition_srv.response.models.size() == 0)
    {
    	ROS_ERROR_STREAM(NODE_NAME<<": Recognition found no objects");
    	return false;
    }

    ROS_INFO_STREAM(NODE_NAME<<": Recognition took " << (ros::WallTime::now()-after_seg));
    ROS_INFO_STREAM(NODE_NAME<<": Got " << recognition_srv.response.models.size() << " models");
    recognized_models_ = recognition_srv.response.models;
	std::stringstream stdout;
	stdout<<"Retrieved models:";
    BOOST_FOREACH(household_objects_database_msgs::DatabaseModelPose model,recognition_srv.response.models[0].model_list)
    {
    	stdout<<"\n\tModel id: "<<model.model_id;
    	stdout<<"\n\tPose frame id: "<<model.pose.header.frame_id;
    	stdout<<"\n\tdetector name: "<<model.detector_name;
    	stdout<<"\n";
    }
    ROS_INFO_STREAM(stdout.str());


    for(unsigned int i = 0; i < 1; i++)
    {
      got_recognition = true;
      addDetectedObjectToPlanningSceneDiff(recognition_srv.response.models[0]);
    }
  }

  perception_duration_ += ros::WallTime::now()-start;
  return got_recognition;
}

bool RobotPickPlaceNavigator::recognize()
{
	ros::WallTime start = ros::WallTime::now();
	planning_scene_diff_.collision_objects.clear();
	recognized_obj_pose_map_.clear();

	// calling segmentation
	tabletop_object_detector::TabletopSegmentation segmentation_srv;
	if (!seg_srv_.call(segmentation_srv))
	{
	ROS_ERROR("Call to segmentation service failed");
	return false;
	}

	ros::WallTime after_seg = ros::WallTime::now();
	bool success = false;

	tabletop_object_detector::TabletopObjectRecognition recognition_srv;
	recognition_srv.request.table = segmentation_srv.response.table;
	recognition_srv.request.clusters = segmentation_srv.response.clusters;
	recognition_srv.request.num_models = 1;
	recognition_srv.request.perform_fit_merge = false;
	if (!rec_srv_.call(recognition_srv))
	{
		ROS_ERROR_STREAM("Call to recognition service " <<recognition_service_.c_str()<<" failed.");
	}

	ROS_INFO_STREAM("Recognition took " << (ros::WallTime::now()-after_seg));
	ROS_INFO_STREAM("Got " << recognition_srv.response.models.size() << " models");
	recognized_models_ = recognition_srv.response.models;

	std::stringstream ss;
	for(unsigned int i = 0; i < recognized_models_.size(); i++)
	{
		success = true;
		ss<<"\n\nRecognized model #"<<i+1<<" details:"<<"\n\tName:\t"<<recognized_models_[i].model_list[0].detector_name;
		ss<<"\n\tModel Id:\t"<<recognized_models_[i].model_list[0].model_id;
		ss<<"\n\tConfidence:\t"<<recognized_models_[i].model_list[0].confidence;

	}

	//convert returned service response to rviz markers

	perception_duration_ += ros::WallTime::now()-start;
	return success;
}

bool RobotPickPlaceNavigator::getMeshFromDatabasePose(const household_objects_database_msgs::DatabaseModelPose &model_pose,
                             arm_navigation_msgs::CollisionObject& obj,
                             const geometry_msgs::PoseStamped& pose)
{
  household_objects_database_msgs::GetModelMesh::Request req;
  household_objects_database_msgs::GetModelMesh::Response res;

  req.model_id = model_pose.model_id;
  if(!object_database_model_mesh_client_.call(req, res))
  {
    ROS_WARN_STREAM("Call to objects database for getMesh failed");
    return false;
  }
  if(res.return_code.code != res.return_code.SUCCESS) {
    ROS_WARN_STREAM("Object database gave non-success code " << res.return_code.code << " for model id " << req.model_id);
    return false;
  }

  obj.header = pose.header;
  obj.poses.resize(1);
  obj.shapes.resize(1);

  recognized_obj_pose_map_[obj.id] = pose;

  bool use_cylinder = true;

  if(use_cylinder) {
    tf::Transform pose_tf;
    tf::poseMsgToTF(pose.pose, pose_tf);

    shapes::Shape* shape = planning_environment::constructObject(res.mesh);
    bodies::Body* body = bodies::createBodyFromShape(shape);
    body->setPose(pose_tf);

    bodies::BoundingCylinder cyl;
    body->computeBoundingCylinder(cyl);

    obj.shapes[0].type = arm_navigation_msgs::Shape::CYLINDER;
    obj.shapes[0].dimensions.resize(2);
    obj.shapes[0].dimensions[0] = cyl.radius;
    obj.shapes[0].dimensions[1] = cyl.length;
    tf::poseTFToMsg(cyl.pose, obj.poses[0]);
  } else {
    obj.shapes.resize(1);
    obj.shapes[0].type = arm_navigation_msgs::Shape::MESH;
    obj.shapes[0] = res.mesh;
    obj.poses[0] = pose.pose;
  }
  return true;
}

bool RobotPickPlaceNavigator::selectGraspableObject(const std::string& arm_name,
                           object_manipulation_msgs::PickupGoal& pickup_goal,
                           std::vector<object_manipulation_msgs::Grasp>& grasps)
{
  std::stringstream logStream;

  if(recognized_models_.size() == 0) return false;
  if(recognized_models_[0].model_list.size() == 0) return false;

  household_objects_database_msgs::DatabaseModelPose& dmp = recognized_models_[0].model_list[0];

  pickup_goal.arm_name = arm_name;
  pickup_goal.collision_object_name = makeCollisionObjectNameFromModelId(dmp.model_id);
  pickup_goal.lift.direction.header.frame_id = cm_.getWorldFrameId();
  pickup_goal.lift.direction.vector.z = 1.0;
  pickup_goal.lift.desired_distance = .1;
  pickup_goal.target.reference_frame_id = pickup_goal.collision_object_name;
  pickup_goal.target.cluster = segmented_clusters_[0];
  pickup_goal.allow_gripper_support_collision = true;
  pickup_goal.collision_support_surface_name = "table";

  // printing request
  logStream<<"\nfreetail manipulation: "<<"Household object database query request"<<"\n";
  logStream<<"freetail manipulation: "<<"\t-Arm Name:\t"<<pickup_goal.arm_name<<"\n";
  logStream<<"freetail manipulation: "<<"\t-Collision Object Name:\t"<<pickup_goal.collision_object_name<<"\n";
  logStream<<"freetail manipulation: "<<"\t-Target Ref Frame Id:\t"<<pickup_goal.target.reference_frame_id<<"\n";
  logStream<<"freetail manipulation: "<<"\t-World Ref Frame Id:\t"<<pickup_goal.lift.direction.header.frame_id<<"\n";

  ROS_INFO("%s",logStream.str().c_str());

  household_objects_database_msgs::DatabaseModelPose dmp_copy = dmp;
  dmp_copy.pose = recognized_obj_pose_map_[pickup_goal.collision_object_name];
  pickup_goal.target.potential_models.push_back(dmp_copy);

  return getObjectGrasps(arm_name, dmp, segmented_clusters_[0], grasps);
}

bool RobotPickPlaceNavigator::performGraspPlanning()
{
	//  ===================================== saving current time stamp =====================================

	//  ===================================== clearing results from last call =====================================

	//  ===================================== calling service =====================================

	// ===================================== checking results ========================================

	//  ===================================== storing results =====================================

	//  ===================================== updating local planning scene =====================================

	// ===================================== printing completion info message =====================================

	return true;
}

bool RobotPickPlaceNavigator::placeAtGoalLocation(const std::string &armName)
{
	// resetting scene
	getAndSetPlanningScene();

	// getting parameters
	_GoalParameters.fetchParameters(GOAL_NAMESPACE);

	// starting timer
	ros::WallTime start_time = ros::WallTime::now();

	// cancel if object is not held by gripper
	if(!object_in_hand_map_[armName]) return false;

	// find transform of wrist relative to the gripper's tcp
	tf::StampedTransform gripperTcpToWrist = tf::StampedTransform();
	_TfListener.lookupTransform(gripper_link_name_,wrist_link_name_,ros::Time(0),gripperTcpToWrist);

	object_manipulation_msgs::PlaceGoal place_goal;
	place_goal.arm_name = armName;
	place_goal.grasp = current_grasp_map_[armName];
	place_goal.desired_retreat_distance = .1;
	place_goal.min_retreat_distance = .1;
	place_goal.approach.desired_distance = .1;
	place_goal.approach.min_distance = .1;
	place_goal.approach.direction.header.frame_id = cm_.getWorldFrameId();
	place_goal.approach.direction.vector.x = 0.0;
	place_goal.approach.direction.vector.y = 0.0;
	place_goal.approach.direction.vector.z = -1.0;
	place_goal.collision_object_name = "attached_"+current_grasped_object_name_[armName];
	place_goal.allow_gripper_support_collision = true;
	place_goal.collision_support_surface_name = "table";
	place_goal.place_padding = .02;

	// creating place locations array
	int numCandidatePoses = _GoalParameters.NumGoalCandidates;
	const tf::Vector3 &axisRotation = _GoalParameters.Axis;
	std::vector<geometry_msgs::PoseStamped> place_locations;
	geometry_msgs::PoseStamped pose;
	pose.header.frame_id = _GoalParameters.GoalTransform.frame_id_;

	// rotate about z axis and apply to original goal pose in order to create candidates;
	for(int i = 0; i < numCandidatePoses; i++)
	{
		double ratio = ((double)i)/((double)numCandidatePoses);
		double angle = 2*M_PI*ratio;
		tf::Quaternion q = tf::Quaternion(axisRotation,angle);
		tf::Vector3 p = tf::Vector3(0,0,0);
		tf::Transform candidateTransform = _GoalParameters.GoalTransform*tf::Transform(q,p);
		tf::poseTFToMsg(candidateTransform,pose.pose);
		place_locations.push_back(pose);

		ROS_INFO_STREAM(NODE_NAME<<" :Goal Position.Pos = ["<<pose.pose.position.x<<", "
				<<pose.pose.position.y<<", "<<pose.pose.position.z<<"]");
		ROS_INFO_STREAM(NODE_NAME<<" :Goal Position.Frame_Id = "<<pose.header.frame_id);
	}


	std::vector<object_manipulator::PlaceExecutionInfo> place_execution_info;
	{
		planning_models::KinematicState state(*current_robot_state_);
		place_tester_->setTcpToWristTransform(gripperTcpToWrist);
		place_tester_->setPlanningSceneState(&state);
		place_tester_->testPlaces(place_goal,
								  place_locations,
								  place_execution_info,
								  true);
	}

	grasp_planning_duration_ += ros::WallTime::now()-start_time;

	for(unsigned int i = 0; i < place_execution_info.size(); i++)
	{
		if(place_execution_info[i].result_.result_code == object_manipulation_msgs::PlaceLocationResult::SUCCESS)
		{
		  current_place_location_ = place_locations[i];
		  bool placed = attemptPlaceSequence(armName, place_execution_info[i]);

		  if(!placed)
		  {
			ROS_WARN_STREAM(NODE_NAME<<": Place failed");
		  }
		  else
		  {
			return true;
		  }
		}
		else
		{
			//ROS_WARN_STREAM(NODE_NAME<<": Goal Location "<< i + 1 <<" is unreachable");
		}
	}

	return false;
}

bool RobotPickPlaceNavigator::putDownSomething(const std::string& arm_name) {

  getAndSetPlanningScene();

  ros::WallTime start_time = ros::WallTime::now();

  if(!object_in_hand_map_[arm_name]) return false;

  tf::StampedTransform gripperTcpToWrist = tf::StampedTransform();
  _TfListener.lookupTransform(gripper_link_name_,wrist_link_name_,ros::Time(0),gripperTcpToWrist);

  object_manipulation_msgs::PlaceGoal place_goal;
  place_goal.arm_name = arm_name;
  place_goal.grasp = current_grasp_map_[arm_name];
  place_goal.desired_retreat_distance = .1;
  place_goal.min_retreat_distance = .1;
  place_goal.approach.desired_distance = .1;
  place_goal.approach.min_distance = .1;
  place_goal.approach.direction.header.frame_id = cm_.getWorldFrameId();
  place_goal.approach.direction.vector.x = 0.0;
  place_goal.approach.direction.vector.y = 0.0;
  place_goal.approach.direction.vector.z = -1.0;
  place_goal.collision_object_name = "attached_"+ current_grasped_object_name_[arm_name];
  place_goal.allow_gripper_support_collision = true;
  place_goal.collision_support_surface_name = "table";
  place_goal.place_padding = .02;

  geometry_msgs::PoseStamped table_pose;
  table_pose.pose = table_.poses[0];
  table_pose.header.frame_id = table_.header.frame_id;

  double l = table_.shapes[0].dimensions[0]-.2;
  double w = table_.shapes[0].dimensions[1]-.2;
  double d = table_.shapes[0].dimensions[2];
  //double d = 0.01f;

  double spacing = .4;

  unsigned int lnum = ceil(l/spacing);
  unsigned int wnum = ceil(w/spacing);

  std::vector<double> angles;
  angles.push_back(0);
  angles.push_back(M_PI/4.0);
  angles.push_back(-M_PI/4.0);
  angles.push_back(M_PI/2.0);
  angles.push_back(-M_PI/2.0);

  unsigned int total_place_locations = lnum*wnum*angles.size();

  std::vector<unsigned int> random_numbers(total_place_locations);
  for(unsigned int i = 0; i < total_place_locations; i++) {
    random_numbers[i] = i;
  }
  random_shuffle(random_numbers.begin(), random_numbers.end());

  std::vector<geometry_msgs::PoseStamped> place_locations(total_place_locations);
  //TODO - make sure that poses are actually on the table
  unsigned int cur_ind = 0;
  for(unsigned int i = 0; i < lnum; i++) {
    for(unsigned int j = 0; j < wnum; j++) {
      for(unsigned int k = 0; k < angles.size(); k++, cur_ind++) {
        geometry_msgs::PoseStamped place_pose = table_pose;
        place_pose.pose.position.x += -(l/2.0)+((i*1.0)*spacing);
        place_pose.pose.position.y += -(w/2.0)+((j*1.0)*spacing);
        place_pose.pose.position.z += (d/2.0);
        place_pose.pose.position.z = 0.08f;
        tf::Transform rot(tf::Quaternion(tf::Vector3(0.0,0.0,1.0), angles[k]), tf::Vector3(0.0,0.0,0.0));
        tf::Transform trans;
        tf::poseMsgToTF(place_pose.pose, trans);
        trans = trans*rot;

        //tf::poseTFToMsg(trans*objToTcp*(objToWrist.inverse()), place_pose.pose);
        tf::poseTFToMsg(trans, place_pose.pose);
        ROS_INFO_STREAM("Place location " << i << " " << j << " "
                         << place_pose.pose.position.x << " "
                         << place_pose.pose.position.y << " "
                         << place_pose.pose.position.z);
        place_locations[random_numbers[cur_ind]] = place_pose;
      }
    }
  }

  std::vector<object_manipulator::PlaceExecutionInfo> place_execution_info;
  {
    planning_models::KinematicState state(*current_robot_state_);
    place_tester_->setTcpToWristTransform(gripperTcpToWrist);
    place_tester_->setPlanningSceneState(&state);
    place_tester_->testPlaces(place_goal,
                              place_locations,
                              place_execution_info,
                              true);
  }

  grasp_planning_duration_ += ros::WallTime::now()-start_time;

  for(unsigned int i = 0; i < place_execution_info.size(); i++) {
    if(place_execution_info[i].result_.result_code == object_manipulation_msgs::PlaceLocationResult::SUCCESS)
    {
      current_place_location_ = place_locations[i];
      bool placed = attemptPlaceSequence(arm_name, place_execution_info[i]);
      if(!placed)
      {
        ROS_WARN_STREAM(NODE_NAME<<": Place failed");
      }
      else
      {
        return true;
      }
    }
    else
    {
    	//ROS_WARN_STREAM(NODE_NAME<<": Goal Location "<< i + 1 <<" is unreachable");
    }
  }
  return false;
}

bool RobotPickPlaceNavigator::pickUpSomething(const std::string& arm_name) {

  getAndSetPlanningScene();

  ros::WallTime start_time = ros::WallTime::now();

  object_manipulation_msgs::PickupGoal pickup_goal;
  std::vector<object_manipulation_msgs::Grasp> grasps;
  if(!selectGraspableObject(arm_name,pickup_goal,grasps))
  {
	ROS_INFO("%s","freetail manipulation: graspable object selection failed");
    return false;
  }
  std::vector<object_manipulator::GraspExecutionInfo> grasp_execution_info;

  {
    planning_models::KinematicState state(*current_robot_state_);
    grasp_tester_->setPlanningSceneState(&state);

    ROS_INFO_STREAM(NODE_NAME<<": Pickup goal arm name is " << pickup_goal.arm_name);
    ROS_INFO_STREAM(NODE_NAME<<": Evaluating grasps with grasp tester");
    grasp_tester_->testGrasps(pickup_goal, grasps, grasp_execution_info, true);
    ROS_INFO_STREAM(NODE_NAME<<": Returned "<< grasp_execution_info.size() <<" grasps candidates ");
  }

  grasp_planning_duration_ += ros::WallTime::now()-start_time;

  // finding wrist pose relative to gripper
  tf::StampedTransform gripperTcpToWrist = tf::StampedTransform();
  _TfListener.lookupTransform(gripper_link_name_,wrist_link_name_,ros::Time(0),gripperTcpToWrist);

  for(unsigned int i = 0; i < grasp_execution_info.size(); i++)
  {
    if(grasp_execution_info[i].result_.result_code == object_manipulation_msgs::GraspResult::SUCCESS)
    {
      current_grasped_object_name_[arm_name] = pickup_goal.collision_object_name;

      // temp grasp
      object_manipulation_msgs::Grasp tempGrasp;

      // storing gripper grasp pose (gripper tcp relative to object)
      tf::Transform wristInObjPose;
      tf::poseMsgToTF(grasps[i].grasp_pose,wristInObjPose);
      tf::poseTFToMsg(wristInObjPose*(gripperTcpToWrist.inverse()),tempGrasp.grasp_pose);
      current_grasp_map_[arm_name] = tempGrasp;

      // updating attached objt marker pose
      if(MarkerMap.find(MARKER_ATTACHED_OBJ) != MarkerMap.end())
      {
    	  visualization_msgs::Marker &m = MarkerMap[MARKER_ATTACHED_OBJ];
    	  tf::poseTFToMsg(wristInObjPose.inverse(),m.pose);
    	  m.header.frame_id = gripper_link_name_;
      }

      ROS_INFO_STREAM(NODE_NAME<<": Attempting grasp sequence");
      bool grasped =  attemptGraspSequence(arm_name, grasp_execution_info[i]);

      if(!grasped)
      {
        ROS_WARN_STREAM(NODE_NAME<<": Grasp failed");
        current_grasped_object_name_.erase(arm_name);
        current_grasp_map_.erase(arm_name);
        return false;
      }
      else
      {
    	  ROS_INFO_STREAM(NODE_NAME<<": Attempting grasp sequence succeeded");
    	  return true;
      }
    }
    else
    {
    	ROS_INFO_STREAM(NODE_NAME<<": Grasp candidate "<<i+1<<" is unreachable");
    }
  }
  return false;
}

void RobotPickPlaceNavigator::attachCollisionObjectCallback(const std::string& group_name) {
  ROS_INFO_STREAM(NODE_NAME<<": In attach callback");
  attachCollisionObject(group_name,
                        current_grasped_object_name_[group_name],
                        current_grasp_map_[group_name]);
  object_in_hand_map_[group_name] = true;
}

void RobotPickPlaceNavigator::detachCollisionObjectCallback(const std::string& group_name) {
  ROS_INFO_STREAM("In detach callback");
  detachCollisionObject(group_name,
                        current_place_location_,
                        current_grasp_map_[group_name]);
  object_in_hand_map_[group_name] = false;
  current_grasped_object_name_.erase(group_name);
  current_grasp_map_.erase(group_name);
}

bool RobotPickPlaceNavigator::attemptGraspSequence(const std::string& group_name,
                          const object_manipulator::GraspExecutionInfo& gei) {

  std::vector<std::string> segment_names;

  //first open the gripper
  std::vector<TrajectoryExecutionRequest> ter_reqs;
  TrajectoryExecutionRequest gripper_ter;
  gripper_ter.group_name_ = gripper_group_name_;
  gripper_ter.controller_name_ = grasp_action_service_;
  gripper_ter.trajectory_ = getGripperTrajectory(group_name, true);
  gripper_ter.failure_ok_ = true;
  gripper_ter.test_for_close_enough_ = false;
  ter_reqs.push_back(gripper_ter);

  ros::WallTime start_execution = ros::WallTime::now();
  ROS_INFO_STREAM(NODE_NAME << ": Open gripper trajectory in progress");
  trajectory_execution_monitor_.executeTrajectories(ter_reqs,trajectories_finished_function_);
  {
    boost::unique_lock<boost::mutex> lock(execution_mutex_);
    execution_completed_.wait(lock);
  }

  execution_duration_ += (ros::WallTime::now()-start_execution);
  ROS_INFO_STREAM(NODE_NAME << ": Open gripper trajectory completed");
  ter_reqs.clear();

  // moving arm from current configuration to pre-grasp configuration
  updateCurrentJointStateToLastTrajectoryPoint(last_trajectory_execution_data_vector_.back().recorded_trajectory_);
  ROS_INFO_STREAM(NODE_NAME << ": arm approach trajectory in progress");
  if(!moveArm(group_name, gei.approach_trajectory_.points[0].positions))
  {
    return false;
  }
  ROS_INFO_STREAM(NODE_NAME << ": arm approach trajectory completed");
  trajectories_succeeded_ = false;

  // request gripper pre-grasp command
  object_manipulation_msgs::GraspHandPostureExecutionGoal graspGoal;
  graspGoal.goal = object_manipulation_msgs::GraspHandPostureExecutionGoal::PRE_GRASP;

  ROS_INFO_STREAM(NODE_NAME << ": Requesting pre-grasp");
  grasp_exec_action_client_.sendGoal(graspGoal);
  if(!grasp_exec_action_client_.waitForResult(ros::Duration(2.0f)))
  {
	  ROS_ERROR_STREAM(NODE_NAME << ": Pre-grasp request timeout, exiting");
	  return false;
  }

  if(grasp_exec_action_client_.getState() != actionlib::SimpleClientGoalState::SUCCEEDED)
  {
	  ROS_ERROR_STREAM(NODE_NAME << ": Pre-grasp request unsuccessful, exiting");
	  return false;
  }
  else
  {
	  ROS_INFO_STREAM(NODE_NAME << ": Pre-grasp completed");
  }


  //now do approach
  TrajectoryExecutionRequest arm_ter;
  arm_ter.group_name_ = group_name;
  arm_ter.controller_name_ = arm_controller_handler_->getControllerName();
  arm_ter.trajectory_ = gei.approach_trajectory_;
  validateJointTrajectory(arm_ter.trajectory_);
  fastFilterTrajectory(arm_ter.group_name_, arm_ter.trajectory_);
  arm_ter.test_for_close_enough_ = false;
  arm_ter.failure_ok_ = false;
  arm_ter.max_joint_distance_ = .01;
  arm_ter.failure_time_factor_ = 1000;
  arm_ter.callback_function_ = boost::bind(&RobotPickPlaceNavigator::attachCollisionObjectCallback, this, _1);
  ter_reqs.push_back(arm_ter);
  segment_names.push_back("approach");

  //now close the gripper
  gripper_ter.trajectory_ = getGripperTrajectory(group_name, false);
  validateJointTrajectory(gripper_ter.trajectory_);
  ter_reqs.push_back(gripper_ter);
  segment_names.push_back("close");

  // updating attached  marker operation
  if(MarkerMap.find(MARKER_ATTACHED_OBJ) != MarkerMap.end())
  {
	  visualization_msgs::Marker &m = MarkerMap[MARKER_ATTACHED_OBJ];
	  m.action = visualization_msgs::Marker::ADD;
  }

  //and do the lift
  arm_ter.trajectory_ = gei.lift_trajectory_;
  validateJointTrajectory(arm_ter.trajectory_);
  fastFilterTrajectory(group_name, arm_ter.trajectory_);
  arm_ter.callback_function_ = NULL;
  ter_reqs.push_back(arm_ter);
  segment_names.push_back("lift");

  /*
   * executing all trajectories
   * currently this is the only way to execute multiple trajectories in sequence, since the executeTrajectories
   * method can only handle a single trajectory at a time.
   */
  start_execution = ros::WallTime::now();
  for(unsigned int i = 0;i < ter_reqs.size(); i++)
  {
	  std::vector<TrajectoryExecutionRequest> tempRequest;
	  tempRequest.push_back(ter_reqs[i]);
	  ROS_INFO_STREAM(NODE_NAME << ": gripper "<<segment_names[i] <<" trajectory in progress");
	  trajectory_execution_monitor_.executeTrajectories(tempRequest,
														trajectories_finished_function_);
	  {
		  boost::unique_lock<boost::mutex> lock(execution_mutex_);
		  execution_completed_.wait(lock);
	  }
	  ROS_INFO_STREAM(NODE_NAME << ": gripper "<<segment_names[i] <<" trajectory completed");
  }

  execution_duration_ += (ros::WallTime::now()-start_execution);

  return trajectories_succeeded_;
}

bool RobotPickPlaceNavigator::attemptPlaceSequence(const std::string& group_name,
                          const object_manipulator::PlaceExecutionInfo& pei) {
  std::vector<TrajectoryExecutionRequest> ter_reqs;

  if(!moveArm(group_name,
              pei.descend_trajectory_.points[0].positions)) {
    return false;
  }

  trajectories_succeeded_ = false;
  std::vector<std::string> segment_names;

  //now do descend
  TrajectoryExecutionRequest arm_ter;
  arm_ter.group_name_ = group_name;
  arm_ter.controller_name_ = arm_controller_handler_->getControllerName();
  arm_ter.trajectory_ = pei.descend_trajectory_;
  validateJointTrajectory(arm_ter.trajectory_);
  fastFilterTrajectory(group_name, arm_ter.trajectory_);
  arm_ter.test_for_close_enough_ = false;
  arm_ter.max_joint_distance_ = .05;
  arm_ter.failure_time_factor_ = 1000;
  arm_ter.callback_function_ = boost::bind(&RobotPickPlaceNavigator::detachCollisionObjectCallback, this, _1);
  ter_reqs.push_back(arm_ter);
  segment_names.push_back("descend");

  //open gripper
  TrajectoryExecutionRequest gripper_ter;
  gripper_ter.group_name_ = gripper_group_name_;
  gripper_ter.controller_name_ = grasp_action_service_;
  gripper_ter.trajectory_ = getGripperTrajectory(group_name, true);
  validateJointTrajectory(gripper_ter.trajectory_);
  gripper_ter.failure_ok_ = true;
  gripper_ter.test_for_close_enough_ = false;
  ter_reqs.push_back(gripper_ter);
  segment_names.push_back("open_gripper");

  // updating attached objt marker operation
  if(MarkerMap.find(MARKER_ATTACHED_OBJ) != MarkerMap.end())
  {
	  visualization_msgs::Marker &m = MarkerMap[MARKER_ATTACHED_OBJ];
	  m.action = visualization_msgs::Marker::DELETE;
  }

  //do the retreat
  arm_ter.trajectory_ = pei.retreat_trajectory_;
  validateJointTrajectory(arm_ter.trajectory_);
  fastFilterTrajectory(group_name, arm_ter.trajectory_);
  arm_ter.callback_function_ = 0;
  ter_reqs.push_back(arm_ter);
  segment_names.push_back("retreat");

  /*
   * executing all trajectories
   * currently this is the only way to execute multiple trajectories in sequence, since the executeTrajectories
   * method can only handle a single trajectory at a time.
   */
  ros::WallTime start_execution = ros::WallTime::now();
  for(unsigned int i = 0;i < ter_reqs.size(); i++)
  {
	  std::vector<TrajectoryExecutionRequest> tempRequest;
	  tempRequest.push_back(ter_reqs[i]);
	  ROS_INFO_STREAM(NODE_NAME << ": gripper "<<segment_names[i] <<" trajectory in progress");
	  trajectory_execution_monitor_.executeTrajectories(tempRequest,
														trajectories_finished_function_);
	  {
		  boost::unique_lock<boost::mutex> lock(execution_mutex_);
		  execution_completed_.wait(lock);
	  }
	  ROS_INFO_STREAM(NODE_NAME << ": gripper "<<segment_names[i] <<" trajectory completed");
  }

  execution_duration_ += (ros::WallTime::now()-start_execution);
  return trajectories_succeeded_;
}

bool RobotPickPlaceNavigator::getObjectGrasps(const std::string& arm_name,
                     const household_objects_database_msgs::DatabaseModelPose& dmp,
                     const sensor_msgs::PointCloud& cloud,
                     std::vector<object_manipulation_msgs::Grasp>& grasps)
{
  household_objects_database_msgs::GetModelDescription::Request des_req;
  household_objects_database_msgs::GetModelDescription::Response des_res;

  des_req.model_id = dmp.model_id;

  if(ros::service::exists(model_database_service_,false))
  {
	  if(!object_database_model_description_client_.call(des_req, des_res))
	  {
		ROS_WARN_STREAM("Call to objects database for getModelDescription failed");
		return false;
	  }

	  if(des_res.return_code.code != des_res.return_code.SUCCESS)
	  {
		ROS_WARN_STREAM("Object database gave non-success code " << des_res.return_code.code << " for model id " << des_req.model_id);
		return false;
	  }

	  ROS_INFO_STREAM(NODE_NAME<<" model database service returned description with name: "<<des_res.name);
  }
  else
  {
	  ROS_INFO_STREAM(NODE_NAME<<": Model database service "<< model_database_service_
			  <<" not found, skipping service call");
  }

  household_objects_database_msgs::DatabaseModelPose dmp_copy = dmp;
  dmp_copy.pose = geometry_msgs::PoseStamped();
  dmp_copy.pose.pose.orientation.w = 1.0;
  dmp_copy.pose.header.frame_id = des_res.name;
  dmp_copy.pose.header.stamp = ros::Time::now();


  object_manipulation_msgs::GraspPlanning::Request request;
  object_manipulation_msgs::GraspPlanning::Response response;

  request.arm_name = arm_name;
  request.target.potential_models.push_back(dmp_copy);
  //request.target.reference_frame_id = des_res.name;
  request.target.reference_frame_id = cloud.header.frame_id;
  request.target.cluster = cloud;

  if(!grasp_planning_client.call(request, response))
  {
    ROS_WARN_STREAM(NODE_NAME<<": Call to objects database for GraspPlanning failed");
    return false;
  }

  if(response.error_code.value != response.error_code.SUCCESS)
  {
    ROS_WARN_STREAM(NODE_NAME<<": Object database gave non-success code " << response.error_code.value);
    return false;
  }

  // verify grasp plan results
  if(response.grasps.size() == 0)
  {
    ROS_WARN_STREAM(NODE_NAME<<": No grasps returned in response");
    return false;
  }
  if(response.grasps.size() > 0)
  {
    ROS_INFO_STREAM(NODE_NAME<<": Grasp is " << response.grasps[0].grasp_pose.position.x << " "
		      << response.grasps[0].grasp_pose.position.y << " "
		      << response.grasps[0].grasp_pose.position.z);
  }

  ROS_INFO_STREAM(NODE_NAME<<": Cloud header " << cloud.header.frame_id);
  ROS_INFO_STREAM(NODE_NAME<<": Recognition pose frame " << dmp.pose.header.frame_id);

  tf::Transform first_grasp_in_world_tf;
  tf::poseMsgToTF(response.grasps[0].grasp_pose, first_grasp_in_world_tf);

  ROS_INFO_STREAM(NODE_NAME<<": Recognition pose frame " << dmp.pose.header.frame_id);
  planning_models::KinematicState state(*current_robot_state_);
  state.updateKinematicStateWithLinkAt(gripper_link_name_, first_grasp_in_world_tf);

  std_msgs::ColorRGBA col_pregrasp;
  col_pregrasp.r = 0.0;
  col_pregrasp.g = 1.0;
  col_pregrasp.b = 1.0;
  col_pregrasp.a = 1.0;
  visualization_msgs::MarkerArray arr;

  ROS_INFO_STREAM(NODE_NAME<<": Getting kinematic model group under 'end_effector'");
  std::vector<std::string> links = cm_.getKinematicModel()->getModelGroup(gripper_group_name_)->getGroupLinkNames();

  ROS_INFO_STREAM(NODE_NAME<<": Obtaining robot marker from state and publishing");
  cm_.getRobotMarkersGivenState(state, arr, col_pregrasp,"first_grasp", ros::Duration(0.0), &links);
  vis_marker_array_publisher_.publish(arr);

  //TODO - actually deal with the different cases here, especially for the cluster planner
  ROS_INFO_STREAM(NODE_NAME<<": Published markers");
  grasps = response.grasps;
  if(request.target.reference_frame_id != des_res.name)
  {
    if(request.target.reference_frame_id != dmp.pose.header.frame_id)
    {
      ROS_WARN_STREAM("Cluster does not match recognition");
    }
    else
    {
      tf::Transform object_in_world_tf;
      tf::poseMsgToTF(dmp.pose.pose, object_in_world_tf);


      //poses are positions of the wrist link in terms of the world
      //we need to get them in terms of the object

      // needs to apply this transform so that the frame of the arm wrist relative to the object is obtained
      tf::StampedTransform wristInGripperTcp = tf::StampedTransform();
      _TfListener.lookupTransform(gripper_link_name_,wrist_link_name_,ros::Time(0),wristInGripperTcp);

      // object pose
      tf::Transform object_in_world_inverse_tf = object_in_world_tf.inverse();

      for(unsigned int i = 0; i < grasps.size(); i++)
      {
        tf::Transform grasp_in_world_tf;
        tf::poseMsgToTF(grasps[i].grasp_pose, grasp_in_world_tf);
        tf::poseTFToMsg(object_in_world_inverse_tf*(grasp_in_world_tf*wristInGripperTcp), grasps[i].grasp_pose);
      }
    }
  }
  return true;
}

trajectory_msgs::JointTrajectory RobotPickPlaceNavigator::getGripperTrajectory(const std::string& arm_name,bool open)
{
  trajectory_msgs::JointTrajectory gt;
  if(arm_name == "right_arm") {
    gt.joint_names.push_back("r_gripper_l_finger_joint");
    gt.joint_names.push_back("r_gripper_r_finger_joint");
    gt.joint_names.push_back("r_gripper_r_finger_tip_joint");
    gt.joint_names.push_back("r_gripper_l_finger_tip_joint");
  } else {
    gt.joint_names.push_back("l_gripper_l_finger_joint");
    gt.joint_names.push_back("l_gripper_r_finger_joint");
    gt.joint_names.push_back("l_gripper_r_finger_tip_joint");
    gt.joint_names.push_back("l_gripper_l_finger_tip_joint");
  }
  gt.points.resize(1);
  if(open) {
    gt.points[0].positions.resize(4, .1);
  } else {
    gt.points[0].positions.resize(4, 0.0);
  }
  return gt;
}

bool RobotPickPlaceNavigator::attachCollisionObject(const std::string& group_name,
                           const std::string& collision_object_name,
                           const object_manipulation_msgs::Grasp& grasp) {

  // removing object from planning scene first if it is found
  arm_navigation_msgs::AttachedCollisionObject att;
  bool found = false;
  for(std::vector<arm_navigation_msgs::CollisionObject>::iterator it = planning_scene_diff_.collision_objects.begin();
      it != planning_scene_diff_.collision_objects.end();
      it++)
  {

    if((*it).id == collision_object_name)
    {
      found = true;
      att.object = (*it);
      planning_scene_diff_.collision_objects.erase(it);
      break;
    }
  }


  if(!found)
  {
    ROS_ERROR_STREAM("No object with id " << collision_object_name);
    return false;
  }

  att.link_name = gripper_link_name_;
  att.touch_links.push_back(gripper_group_name_);

  planning_models::KinematicState state(*current_robot_state_);
  tf::Transform t;
  tf::poseMsgToTF(grasp.grasp_pose, t);

  std::map<std::string, double> grasp_vals;
  for(unsigned int i = 0; i < grasp.grasp_posture.name.size(); i ++)
  {
     grasp_vals[grasp.grasp_posture.name[i]] = grasp.grasp_posture.position[i];
  }
  state.setKinematicState(grasp_vals);

  tf::Transform obj_pose;
  tf::poseMsgToTF(recognized_obj_pose_map_[collision_object_name].pose, obj_pose);// store object pose in world coordinates

  //assumes that the grasp is in the object frame
  tf::Transform grasp_pose = obj_pose*t;

  // //need to do this second, otherwise it gets wiped out
  state.updateKinematicStateWithLinkAt(gripper_link_name_,
                                       grasp_pose);
  // ROS_DEBUG_STREAM("Object pose is frame " << att.object.header.frame_id << " "
  //                 << att.object.poses[0].position.x << " "
  //                 << att.object.poses[0].position.y << " "
  //                 << att.object.poses[0].position.z);

  // ROS_DEBUG_STREAM("Grasp pose is "
  //                  << grasp.grasp_pose.position.x << " "
  //                  << grasp.grasp_pose.position.y << " "
  //                  << grasp.grasp_pose.position.z);

  // tf::Transform trans = state.getLinkState(att.link_name)->getGlobalLinkTransform();

  // ROS_DEBUG_STREAM("Fingertip pose is "
  //                  << trans.getOrigin().x() << " "
  //                  << trans.getOrigin().y() << " "
  //                  << trans.getOrigin().z());


  geometry_msgs::PoseStamped ps;
  //now we need to get the object pose in terms of the gripper
  cm_.convertPoseGivenWorldTransform(state,
                                     att.link_name,
                                     att.object.header,
                                     att.object.poses[0],
                                     ps);

  att.object.id = "attached_"+att.object.id;
  att.object.header = ps.header;
  att.object.poses[0] = ps.pose;

  ROS_INFO_STREAM("object pose is relative to " + gripper_link_name_ + ": "
                  << ps.pose.position.x << " "
                  << ps.pose.position.y << " "
                  << ps.pose.position.z);

  ROS_INFO_STREAM("Attaching  " << att.object.id << " mode " << att.object.operation.operation);
  planning_scene_diff_.attached_collision_objects.push_back(att);

  attached_object_publisher_.publish(att);
  return true;
}

bool RobotPickPlaceNavigator::detachCollisionObject(const std::string& group_name,
                           const geometry_msgs::PoseStamped& place_pose,
                           const object_manipulation_msgs::Grasp& grasp) {

  ROS_INFO_STREAM("Place pose is " << place_pose.header.frame_id);

  planning_models::KinematicState state(*current_robot_state_);
  tf::Transform place_pose_tf;
  tf::poseMsgToTF(place_pose.pose, place_pose_tf);

  tf::Transform grasp_trans;
  tf::poseMsgToTF(grasp.grasp_pose, grasp_trans);

  place_pose_tf = place_pose_tf*grasp_trans;

  state.updateKinematicStateWithLinkAt(gripper_link_name_,
                                       place_pose_tf);

  if(planning_scene_diff_.attached_collision_objects.size() == 0) {
    ROS_ERROR_STREAM("No attached bodies in scene");
    return false;
  }

  arm_navigation_msgs::CollisionObject& obj =  planning_scene_diff_.attached_collision_objects.front().object;
  const planning_models::KinematicState::AttachedBodyState* att = state.getAttachedBodyState(obj.id);
  if(att == NULL) {
    ROS_ERROR_STREAM("No attached body model " << obj.id << " in attached");
    return false;
  }

  arm_navigation_msgs::AttachedCollisionObject att_publish;
  att_publish.object.id = obj.id;
  att_publish.object.operation.operation = arm_navigation_msgs::CollisionObjectOperation::REMOVE;
  attached_object_publisher_.publish(att_publish);

  obj.header.frame_id = cm_.getWorldFrameId();
  tf::poseTFToMsg(att->getGlobalCollisionBodyTransforms()[0], obj.poses[0]);
  obj.id = current_grasped_object_name_[group_name];
  planning_scene_diff_.collision_objects.push_back(obj);
  planning_scene_diff_.attached_collision_objects.clear();
  return true;
}

void RobotPickPlaceNavigator::callbackPublishMarkers(const ros::TimerEvent &evnt)
{
	for(std::map<std::string,visualization_msgs::Marker>::iterator i = MarkerMap.begin(); i != MarkerMap.end(); i++)
	{
		visualization_msgs::Marker &m = i->second;
		m.header.stamp = ros::Time::now();
		vis_marker_publisher_.publish(m);
	}
}

void RobotPickPlaceNavigator::collisionObjToMarker(const arm_navigation_msgs::CollisionObject &obj,visualization_msgs::Marker &marker)
{
	// only converts to a sphere marker for now
	const arm_navigation_msgs::Shape &shape = obj.shapes[0];

	switch(shape.type)
	{
		case arm_navigation_msgs::Shape::SPHERE:

			marker.header.frame_id = obj.header.frame_id;
			marker.type = visualization_msgs::Marker::SPHERE;
			marker.pose = obj.poses[0];
			marker.scale.x = 2*shape.dimensions[0];// 2 x radius
			marker.scale.y =2*shape.dimensions[0];
			marker.scale.z =2*shape.dimensions[0];
			marker.color.a = 1.0;
			marker.color.r = 0.0;
			marker.color.g = 1.0;
			marker.color.b = 0.0;

			break;

		default:

			break;

	}

}

void RobotPickPlaceNavigator::startCycleTimer() {
  execution_duration_ = ros::WallDuration(0.0);
  perception_duration_ = ros::WallDuration(0.0);
  planning_scene_duration_ = ros::WallDuration(0.0);
  motion_planning_duration_ = ros::WallDuration(0.0);
  trajectory_filtering_duration_ = ros::WallDuration(0.0);
  grasp_planning_duration_ = ros::WallDuration(0.0);
  cycle_start_time_ = ros::WallTime::now();
}

void RobotPickPlaceNavigator::printTiming()
{
    ros::WallDuration dur = ros::WallTime::now()-cycle_start_time_;
    ROS_INFO_STREAM("Cycle took " << dur.toSec() << " processing " << (dur-execution_duration_).toSec() << " execution " << execution_duration_.toSec());
    ROS_INFO_STREAM("Perception " << perception_duration_.toSec());
    ROS_INFO_STREAM("Planning scene " << planning_scene_duration_.toSec());
    ROS_INFO_STREAM("Planning time " << motion_planning_duration_.toSec());
    ROS_INFO_STREAM("Filtering time " << trajectory_filtering_duration_.toSec());
    ROS_INFO_STREAM("Grasp planning time " << grasp_planning_duration_.toSec());
  }

std::string RobotPickPlaceNavigator::makeCollisionObjectNameFromModelId(unsigned int model_id)
{
  std::stringstream object_id;
  object_id << "object_" << model_id;
  return object_id.str();
}

const arm_navigation_msgs::CollisionObject* RobotPickPlaceNavigator::getCollisionObject(unsigned int model_id)
{
    std::string name = makeCollisionObjectNameFromModelId(model_id);
    for(unsigned int i = 0; i < planning_scene_diff_.collision_objects.size(); i++) {
      if(planning_scene_diff_.collision_objects[i].id == name) return &(planning_scene_diff_.collision_objects[i]);
    }
    return NULL;
}

void RobotPickPlaceNavigator::run()
{
	switch(configuration_type_)
	{
	case SETUP_FULL:
		runFullNavigation();
		break;

	case SETUP_SPHERE_PICK_PLACE:
		runSpherePickingDemo();
		break;

	default:
		break;

	}
}

void RobotPickPlaceNavigator::runFullNavigation()
{
	// getting and storing node name
	NODE_NAME = ros::this_node::getName();

	ros::AsyncSpinner spinner(4);
	spinner.start();
	srand(time(NULL));

	setup();// full setup

	//ros::NodeHandle nh;
	moveArmToSide();

	while(ros::ok())
	{
	    startCycleTimer();

	    ROS_INFO_STREAM(NODE_NAME + ": Segmentation and recognition stage started");
	    if(!segmentAndRecognize())
	    {
	      ROS_WARN_STREAM(NODE_NAME<<": Segment and recognized failed");
	      continue;
	    }
	    ROS_INFO_STREAM(NODE_NAME << " Segmentation and recognition stage completed");

	    ROS_INFO_STREAM(NODE_NAME << ": grasp pickup stage started");
	    if(!pickUpSomething(arm_group_name_))
	    {
	      ROS_WARN_STREAM(NODE_NAME << ": grasp pickup stage failed");
	      continue;
	    }
	    else
	    {
	    	ROS_INFO_STREAM(NODE_NAME << ": grasp pickup stage completed");
	    }


	    ROS_INFO_STREAM(NODE_NAME + ": grasp place stage started");
	    if(!putDownSomething(arm_group_name_))
	    {
	      ROS_WARN_STREAM(NODE_NAME << ": grasp place stage failed");
	    }
	    else
	    {
	    	ROS_INFO_STREAM(NODE_NAME << ": grasp place stage completed");
	    }

	    if(!moveArmToSide())
	    {
	      ROS_WARN_STREAM(NODE_NAME << ": Final side moved failed");
	      break;
	    }

	    printTiming();
	  }
}

void RobotPickPlaceNavigator::runSpherePickingDemo()
{
	// getting and storing node name
	NODE_NAME = ros::this_node::getName();

	ros::AsyncSpinner spinner(4);
	spinner.start();
	srand(time(NULL));

	setupBallPickingDemo();// full setup

	//ros::NodeHandle nh;
	moveArmToSide();

	while(ros::ok())
	{
	    startCycleTimer();

	    ROS_INFO_STREAM(NODE_NAME << ": Segmentation and recognition stage started");
	    if(!segmentSpheres())
	    {
	      ROS_WARN_STREAM("Segment and recognized failed");
	      continue;
	    }
	    ROS_INFO_STREAM(NODE_NAME << ": Segmentation of spheres stage completed");

	    ROS_INFO_STREAM(NODE_NAME << ": grasp pickup stage started");
	    if(!pickUpSomething(arm_group_name_))
	    {
	      ROS_WARN_STREAM(NODE_NAME << " grasp pickup stage failed");
	      continue;
	    }
	    else
	    {
	    	ROS_INFO_STREAM(NODE_NAME << ": grasp pickup stage completed");
	    }


	    ROS_INFO_STREAM(NODE_NAME << ": grasp place stage started");
	    if(!placeAtGoalLocation(arm_group_name_))
	    {
	      ROS_WARN_STREAM(NODE_NAME << ": grasp place stage failed");
	    }
	    else
	    {
	    	ROS_INFO_STREAM(NODE_NAME << ": grasp place stage completed");
	    }

	    if(!moveArmToSide())
	    {
	      ROS_WARN_STREAM(NODE_NAME << ": Final side moved failed");
	      break;
	    }

	    printTiming();
	  }
}
