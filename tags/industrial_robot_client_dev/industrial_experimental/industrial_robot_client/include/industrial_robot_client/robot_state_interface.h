/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2012, Southwest Research Institute
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


#ifndef ROBOT_STATE_INTERFACE_H
#define ROBOT_STATE_INTERFACE_H

#include "simple_message/smpl_msg_connection.h"
#include "simple_message/message_manager.h"
#include "simple_message/message_handler.h"
#include "simple_message/socket/tcp_client.h"
#include "industrial_robot_client/joint_relay_handler.h"

namespace industrial_robot_client
{
namespace robot_state_interface
{

using industrial::smpl_msg_connection::SmplMsgConnection;
using industrial::message_manager::MessageManager;
using industrial::message_handler::MessageHandler;
using industrial::tcp_client::TcpClient;
using industrial_robot_client::joint_relay_handler::JointRelayHandler;

/**
 * \brief Generic template that reads state-data from a robot controller
 * and publishes matching messages to various ROS topics.
 *
 * Users should replace the default class members
 * to implement robot-specific behavior.
 */
//* RobotStateInterface
class RobotStateInterface
{

public:

  /**
   * \brief Default constructor.
   */
  RobotStateInterface();

  /**
   * \brief Initialize robot connection using default method.
   *
   * \return true on success, false otherwise
   */
  bool init();

  /**
   * \brief Initialize robot connection using specified method.
   *
   * \param new robot-connection instance (ALREADY INITIALIZED).
   *
   * \return true on success, false otherwise
   */
  bool init(SmplMsgConnection* connection);
  /**
   * \brief Begin processing messages and publishing topics.
   */
  void run();

  /**
   * \brief get current robot-connection instance.
   *
   * \return current robot connection object
   */
  SmplMsgConnection* get_connection()
  {
    return this->connection_;
  }

  /**
   * \brief get active message-manager object
   *
   * \return current message-manager object
   */
  MessageManager* get_manager()
  {
    return &this->manager_;
  }


  /**
   * \brief Add a new handler.
   *
   * \param new message-handler for a specific msg-type (ALREADY INITIALIZED).
   * \param replace existing handler (of same msg-type), if exists
   */
  void add_handler(MessageHandler* handler, bool allow_replace = true)
  {
    this->manager_.add(handler, allow_replace);
  }

protected:
  TcpClient default_tcp_connection_;
  JointRelayHandler default_handler_;

  SmplMsgConnection* connection_;
  MessageManager manager_;
};//class RobotStateInterface

}//robot_state_interface
}//industrial_robot_cliet


#endif /* ROBOT_STATE_INTERFACE_H */
