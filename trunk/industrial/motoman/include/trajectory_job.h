/*
* Software License Agreement (BSD License) 
*
* Copyright (c) 2011, Southwest Research Institute
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

#ifndef TRAJECOTRY_JOB_H
#define TRAJECTORY_JOB_H

#ifdef ROS
#include "simple_message/joint_traj.h"
#endif

#ifdef MOTOPLUS
#include "joint_traj.h"
#endif

namespace motoman
{
namespace trajectory_job
{


/**
 * \brief The job class encapsulates a trajectory as defined by a motoman job
 * file (*.JBI).  This class makes certain assumptions about the job file structure
 * and is not meant to be a generic implementation of a job class (but could be extended
 * if the need arises)
 */
//* TrajectoryJob
/**
 *
 * THIS CLASS IS NOT THREAD-SAFE
 *
 */

class TrajectoryJob
{

public:

  /**
* \brief Constructor
*
*/
  TrajectoryJob();

  /**
* \brief Destructor
*
*/
  ~TrajectoryJob();


  /**
* \brief Class initializer
*
* \param job name
* \param joint trajectory
*
* \return true on success, false otherwise (invalid name or trajectory)
*/
bool init(char* name, industrial::joint_traj::JointTraj joint_traj);

/**
* \brief Generates a job string that can be written to a file
*
* \param str_buffer
* \param buffer_size
*
* \return true on success, otherwise false
*/
bool toJobString(char* str_buffer, size_t buffer_size);



private:

/**
 * \brief Size of fixed buffers that hold a line of information (in the JOB file).
 * Allows for static allocation of character buffers
 */
static const int LINE_BUFFER_SIZE_ = 128;

/**
 * \brief Temporary line buffer used when generating a job file test string
 */
char line_buffer_[LINE_BUFFER_SIZE_+1];

/**
 * \brief Size of fixed buffer that holds the job file name.  This is set by the
 * motoman job name size limit
 */
static const int NAME_BUFFER_SIZE_ = 20;
/**
 * \brief Name of the job file
 */
char name_[NAME_BUFFER_SIZE_+1];



/**
 * \brief Joint trajectory
 */
industrial::joint_traj::JointTraj trajectory_;

/**
* \brief Appends a line to the job buffer.  The function checks that the job
* buffer is large enough before this is done.
*
* \param job buffer
* \param job buffer size
* \param line buffer
*
* \return true on success, false if te job buffer is not big enough
*/
bool appendLine(char* job_buffer, size_t job_buffer_size, char* line_buffer);

};

} //trajectory_job
} //motoman

#endif /* TRAJECTORY_JOB_H */
