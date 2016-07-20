#include <fstream> // allow file streaming
#include <regex> // allow regular expressions
#include <sstream> // allow string streams
#include <std_msgs/String.h> // allow string messages

#include <geometry_msgs/Pose.h> // allow pose for move group functionality
#include <moveit/move_group_interface/move_group.h> // allow MoveIt! commands
#include <ros/ros.h> // allow ROS commands

// Namespace Commands and Variables
using namespace ros; // using ROS namespace
namespace planningInterface = moveit::planning_interface; // define commonly used namespace

// Initialize Global Variables
bool writeToFile = 0;
std::string command; // initialize variable for storing the type of point being stored

// Function Prototypes
void commandCallback(std_msgs::String);

std::vector<std::string> addOutputHeader(int, int, std::string);

void writeJoints(planningInterface::MoveGroup&, planningInterface::MoveGroup&, std::string, std::string);
void writePose(planningInterface::MoveGroup&, planningInterface::MoveGroup&, std::string, std::string);
void writeBoth(planningInterface::MoveGroup&, planningInterface::MoveGroup&, std::string, std::string, std::string);

/* -----------------------------------------------
   --------------- MAIN FUNCTION -----------------
   ----------------------------------------------- */
int main(int argc,char **argv) {

	// ========== FUTURE WORK ==========
	// Need to check if the input is a valid file name (doesn't allow regular expressions for some reason) | DATE: 2016-06-17
	// Need to ensure YuMi server and YuMi quick commands have been called in order for this script to work properly
	// =================================

	// Initialize Variables
	std::string output_fileName; // initialize variable to store the full path for the output file
	std::string output_fileName_joints; // initialize variable to store the full path for the output file for outputting joint values if both joints values and poses are desired for output
	std::string output_fileName_poses; // initialize variable to store the full path for the output file for outputting poses if both joints values and poses are desired for output
	std::string output_filePath = "/home/yumi/yumi_ws/src/yumi/yumi_scripts/paths/"; // initializing an output file with default name

	int desiredGroup = -1; // indicates which arm should be used for outputting joints/points: 1) left arm, 2) right arm, 3) both arms
	int outputType = -1; // indicates what function type to call: 1) joints, 2) points, 3) all
	bool programFinished = false; // indicate if the final command has been received and stored

	// Check Inputs
	if (argv[1] == NULL) { // check if an output file name argument was supplied
		ROS_ERROR("An output file name needs to be executed as an additional argument."); // notify user that an argument for the output file name need to be supplied
		return 1; // exit due to error
	}

	if (argv[2] == NULL) { // check if a desired arm argument was supplied
		ROS_ERROR("Need to provide argument to indicate which arm to store values for (left/right/both)."); // notify the user that an argument for the desired arm need to be supplied
		return 1; // exit due to error
	} else { // if a desired arm argument was supplied
		if (strcmp(argv[2],"left") == 0) { desiredGroup = 1; } // if the desired arm is the left arm
		else if (strcmp(argv[2],"right") == 0) { desiredGroup = 2; } // if the desired arm is the right arm
		else if (strcmp(argv[2],"both") == 0) { desiredGroup = 3; }  // if the desired arm is both arms
		else { // if the desired arm argument is not recognized
			ROS_INFO("Input for desired arm(s) is not recognized."); // notify the user that the argument is not recognized
			ROS_INFO("List of valid inputs: left, right, both"); // provide the user with a list of arguments that are recognized
			return 1; // exit due to error
		}
	}

	if (argv[3] == NULL) { // check if an output type argument was supplied
		ROS_ERROR("Need to provide argument to indicate what type of output is desired (joints/pose/all)."); // notfiy the user that an argument for the output type needs to be supplied
		return 1; // exit due to error
	} else { // if an output type argument was supplied
		if (strcmp(argv[3],"joints") == 0) { outputType = 1; } // if the output type argument is joint values
		else if (strcmp(argv[3],"pose") == 0) { outputType = 2; } // if the output type argument is points
		else if (strcmp(argv[3],"both") == 0) { outputType = 3; } // if the output type argument is all
		else { // if the output type argument is not recognized
			ROS_INFO("Input for desired output type is not recognized."); // notify the user that the argument is not recognized
			ROS_INFO("List of valid inputs: joints, pose, all"); // provide the user with a list of arguments that are recognized
			return 1; // exit due to error
		}
	}

	// Get Desired Output File
	std::stringstream fullPath; // initialize variable to concatenate the full path
	fullPath << output_filePath << argv[1] << ".txt"; // concatenate full path
	output_fileName = fullPath.str(); // store the full path

	// Check if Desire Output File Already Exists
	if (std::ifstream(output_fileName)) { // if the desired output file already exists
	     ROS_ERROR("Provided output file name already exists. Please use another name or delete the existing file."); // notify the user that the file already exists
	     ROS_WARN("Provided output file path: %s", output_fileName.c_str()); // notify the user of the full path of the desired output file
	     return 1; // exit the program due to error
	}

	init(argc,argv,"lead_through"); // initialize ROS node

	// Define Move Groups
	planningInterface::MoveGroup right_arm("right_arm"); // initialize structure for right arm move group
	planningInterface::MoveGroup left_arm("left_arm"); // initialize structure for left arm move group
	right_arm.getCurrentJointValues(); // get joint values for right arm since the first time this is executed it produces an empty set, from now on the returned values should be correct
	left_arm.getCurrentJointValues(); // get joint values for left arm since the first time this is executed it produces an empty set, from now on the returned values should be correct

	// Setup Subscriber
	NodeHandle nh; // initialize a node handle
	Subscriber sub = nh.subscribe("lead_through_commands",1000,commandCallback); // subscribe to "lead_through_comamnds" topic

	ROS_INFO(">--------------------"); // add a spacer in output window to make important information stand out 

	// Get Proper File Names and Add Header to File
	std::vector<std::string> output_fileNames = addOutputHeader(outputType, desiredGroup, output_fileName); // add header to the output file
	if (outputType == 3) { // if the user would like to output both the joint values and poses
		output_fileName_joints = output_fileNames[0]; // store the file name for outputting joint values
		output_fileName_poses = output_fileNames[1]; // store the file name for outputting poses
	}

	// Notify User the Program is Ready to Accept Commands
	ROS_INFO("Program ready to accept commands. Run the following command to store position."); // notify the user that program is ready to accept commands
	ROS_INFO("Command: rostopic pub /lead_through_commands std_msgs/String $position_name"); // notify the user of the command to use for sending command to store current position
	ROS_INFO(">--------------------"); // add a spacer in output window to make important information stand out 
	
	// Write Positions/Joints to File
	while(ok()) { // while the ROS node is still in good condition
		if (writeToFile == 1) { // if the user would like to store the current joint positions to the file
			if (outputType == 1) { writeJoints(right_arm, left_arm, command, output_fileName); } // if the user would like to only store the joint values
			else if (outputType == 2) { writePose(right_arm, left_arm, command, output_fileName); } // if the user would like to only store the pose
			else if (outputType == 3) { writeBoth(right_arm, left_arm, command, output_fileName_joints, output_fileName_poses); } // if the user would like to store both the joint values and pose
			writeToFile = 0; // reset flag variable to indicate that the point has been written to file

			if (command.compare("finish") == 0) { // if the last point in the trajectory has been stored
				programFinished = true; // indicate that the final command has been received and stored
				break; // finish the program
			}
		}
		spinOnce(); // allow time to check callbacks
	}

	if (programFinished == true) {
		shutdown();
	} else {
		ROS_WARN("The program was not terminated properly.");
	}

	return 0; // indicate successful main function execution
}


/* -----------------------------------------------
   ------------ SUBSCRIBER FUNCTIONS -------------
   ----------------------------------------------- */
void commandCallback(std_msgs::String msg) {
/*  PROGRAMMER: Frederick Wachter - wachterfreddy@gmail.com
    DATE CREATED: 2016-06-17
    PURPOSE: Store point type from user and indicate that the user would like to store the current point

    INPUTS:
      > msg - string message from "lead_through_commands" topic
    OUTPUT(S): None
*/
	command = msg.data; // take command form topic into local variable
	writeToFile = 1; // indicate that the user would like to write the current point to the file
}

/* -----------------------------------------------
   -------------- GENERAL FUNCTIONS --------------
   ----------------------------------------------- */
std::vector<std::string> addOutputHeader(int outputType, int desiredGroup, std::string output_fileName) {
/*  PROGRAMMER: Frederick Wachter - wachterfreddy@gmail.com
    DATE CREATED: 2016-06-28
    PURPOSE: Add header to output file

    INPUT(S):
      > desiredGroup - desired arm for storing joint values or poses. 1) left arm, 2) right arm, 3) both arms
      > output_fileName - desired file name for the output file
    OUTPUT(S):
      > output_fileNames - vector containing the file names for both joint values and poses output file if the user desires for them both to be outputted
*/
	// Initialize Variables
	std::vector<std::string> output_fileNames(2); // variable to store file names
	std::string intendedGroup; // variable to store the intended arm string

	// Set Inded Arm Variable for Writing to File
	if (desiredGroup == 1) { intendedGroup = "left_arm"; } // if the output is intended only for the left arm
	else if (desiredGroup == 2) { intendedGroup = "right_arm"; } // if the output is intended only for the right arm
	else if (desiredGroup == 3) { intendedGroup = "both_arms"; } // if the output is intended for both arms

	// Open File and Add Header
	if (outputType == 3) { // if the output is intended for both arms
		// Get File Names for Storing Both Joint Values and Poses
		output_fileNames[0] = output_fileName.substr(0,output_fileName.size()-4) + "_joints.txt"; // set special file name for storing joint values for both arms
		output_fileNames[1] = output_fileName.substr(0,output_fileName.size()-4) + "_poses.txt"; // set special file name for storing the pose of both arm

		// Notify User of File Locations
		ROS_INFO("Output file at (joints): %s",output_fileNames[0].c_str()); // notify the user of the full path of the output file for storing joints
		ROS_INFO("Output file at (poses): %s",output_fileNames[1].c_str()); // notify the user of the full path of the output file for storing poses

		// Write Header to the Joint Values File
		std::ofstream output_file_joints; // create file stream variable for joint values
		output_file_joints.open(output_fileNames[0].c_str(), std::ofstream::out | std::ofstream::app); // open file in writing and append mode
		output_file_joints << "joints" << " " << intendedGroup << "\r\n"; // write header to file
		output_file_joints.close(); // close the file

		// Write Header to the Pose File
		std::ofstream output_file_poses; // create file stream variable for poses
		output_file_poses.open(output_fileNames[1].c_str(), std::ofstream::out | std::ofstream::app); // open file in writing and append mode
		output_file_poses << "poses" << " " << intendedGroup << "\r\n"; // write header to file
		output_file_poses.close(); // close the file
	} else { // if the output intended for only one of the arms
		// Notify User of File Location
		ROS_INFO("Output file at: %s",output_fileName.c_str()); // notify the user of the full path of the output file

		// Determine What Type of Data the User Woule Like to Output
		std::string dataType;
		if (outputType == 1) { dataType = "joints"; }
		else if (outputType == 2) { dataType = "poses"; }

		// Write Header to Output File
		std::ofstream output_file; // create file stream variable
		output_file.open(output_fileName.c_str(), std::ofstream::out | std::ofstream::app); // open file in writing and append mode
		output_file << dataType << " " << intendedGroup << "\r\n"; // write header to file
		output_file.close(); // close the file
	}

	return output_fileNames; // return the output files names
}

/* -----------------------------------------------
   ------- WRITE POINTS/JOINTS - BOTH ARMS -------
   ----------------------------------------------- */
void writeJoints(planningInterface::MoveGroup& group1, planningInterface::MoveGroup& group2, std::string command, std::string output_fileName) {
/*  PROGRAMMER: Frederick Wachter - wachterfreddy@gmail.com
    DATE CREATED: 2016-06-17
    PURPOSE: Execute logging for joint locations

    INPUT(S):
      > right_arm - right arm move group
      > left_arm - left arm move group
      > command - string signifying type of point
      > output_fileName - full path of the output file
    OUTPUT(S): None
*/
	// Intiialize Variables
	std::stringstream trajectory_point; // create a string stream for concatenation
	std::ofstream output_file; // create file stream variable

	// Get Joint Values and Total Joints for Each Arm
	std::vector<double> jointValues_group1 = group1.getCurrentJointValues(); // get joint values for right arm
	std::vector<double> jointValues_group2 = group2.getCurrentJointValues(); // get joint values for left arm
	std::vector<double>::size_type totalJoints_group1 = jointValues_group1.size(); // get total amount of joints in right arm
	std::vector<double>::size_type totalJoints_group2  = jointValues_group2.size(); // get total amount of joints in right arm

	// Build Trajectory Point As String
	trajectory_point << command << " "; // add command
	trajectory_point << group1.getName() << " " << totalJoints_group1 << " "; // add header for right arm joints
	for (int i = 0; i < totalJoints_group1; i++) trajectory_point << jointValues_group1[i] << " "; // add right joint values
	trajectory_point << group2.getName() << " " <<  totalJoints_group2 << " "; // add header for left arm joints
	for (int i = 0; i < totalJoints_group2; i++) trajectory_point << jointValues_group2[i] << " "; // add left joint values

	std::string outputLine = trajectory_point.str(); // get stringstream as a string

	// Open File and Add Trajectory Point
	output_file.open(output_fileName.c_str(), std::ofstream::out | std::ofstream::app); // open file in writing and append mode
	output_file << outputLine << "\r\n"; // write trajectory point to file
	output_file.close(); // close the file

	ROS_INFO("Joints stored with command: %s",command.c_str()); // notify user the command was stored
}

void writePose(planningInterface::MoveGroup& group1, planningInterface::MoveGroup& group2, std::string command, std::string output_fileName) {
/*  PROGRAMMER: Frederick Wachter - wachterfreddy@gmail.com
    DATE CREATED: 2016-06-27
    PURPOSE: Execute logging for pose locations

    INPUT(S):
      > right_arm - right arm move group
      > left_arm - left arm move group
      > command - string signifying type of point
      > output_fileName - full path of the output file
    OUTPUT(S): None

    DEPENDENCIES: Assumed the gripper prismatic joint is the last joint in the SRDF for both the left and right arm
*/
	// Intiialize Variables
	std::stringstream trajectory_point; // create a string stream for concatenation
	std::ofstream output_file; // create file stream variable

	// Get Pose of Each Arm
	geometry_msgs::PoseStamped poseMsg_group1 = group1.getCurrentPose(); // get the pose msg for the end effector of the right arm
	geometry_msgs::PoseStamped poseMsg_group2 = group2.getCurrentPose(); // get the pose msg for the end effector of the left arm
	geometry_msgs::Pose pose_group1 = poseMsg_group1.pose; // get the pose from the pose msg for the right arm
	geometry_msgs::Pose pose_group2 = poseMsg_group2.pose; // get the pose from the pose msg for the left arm

	// Determine if Using Grippers
	std::string usingGripper_group1 = "false"; // indicates if a gripper is being used on the first group
	std::string usingGripper_group2 = "false"; // indicates if a gripper is being used on the second group
	double gripperPos_group1, gripperPos_group2; // stores the gripper positions for each group if there is a gripper attached to one or both of the groups

	std::string lastJointName_group1 = group1.getActiveJoints().back(); // get the last active joint name (exclude fixed joints) for the first group
	std::string lastJointName_group2 = group2.getActiveJoints().back(); // get the last active joint name (excludes fixed joints) for the second group
	if (lastJointName_group1.compare(0,7,"gripper") == 0) { // if the last active joint for the first group is a gripper
		usingGripper_group1 = "true"; // indicate the first group has a gripper
		gripperPos_group1 = group1.getCurrentJointValues().back(); // get the gripper position for the first group
	}
	if (lastJointName_group2.compare(0,7,"gripper") == 0) { // if the last active joint for the second group is a gripper
		usingGripper_group2 = "true"; // indicate the second group has a gripper
		gripperPos_group2 = group2.getCurrentJointValues().back(); // get the gripper position for the second group
	}

	// Build Trajectory Point As String
	trajectory_point << command << " "; // add command
	trajectory_point << group1.getName() << " " << usingGripper_group1 << " "; // add group name and whether a gripper is on the first group
	if (usingGripper_group1.compare("true") == 0) { trajectory_point << gripperPos_group1 << " "; } // add gripper position if using a gripper on the first group
	trajectory_point << pose_group1.position.x << " " << pose_group1.position.y << " " << pose_group1.position.z << " "; // add header and position for right arm joints
	trajectory_point << pose_group1.orientation.x << " " << pose_group1.orientation.y << " " << pose_group1.orientation.z << " " << pose_group1.orientation.w << " "; // add orientation for right arm
	trajectory_point << group2.getName() << " " << usingGripper_group2 << " "; // add group name and whether a gripper is on the second group
	if (usingGripper_group2.compare("true") == 0) { trajectory_point << gripperPos_group2 << " "; } // add gripper position if using a gripper on the second group
	trajectory_point << pose_group2.position.x << " " << pose_group2.position.y << " " << pose_group2.position.z << " "; // add header and position for left arm joints
	trajectory_point << pose_group2.orientation.x << " " << pose_group2.orientation.y << " " << pose_group2.orientation.z << " " << pose_group2.orientation.w << " "; // add orientation for left arm

	std::string outputLine = trajectory_point.str(); // get stringstream as a string

	// Open File and Add Trajectory Point
	output_file.open(output_fileName.c_str(), std::ofstream::out | std::ofstream::app); // open file in writing and append mode
	output_file << outputLine << "\r\n"; // write trajectory point to file
	output_file.close(); // close the file

	ROS_INFO("Position stored with command: %s",command.c_str()); // notify user the command was stored
}

void writeBoth(planningInterface::MoveGroup& group1, planningInterface::MoveGroup& group2, std::string command, std::string fileName_joints, std::string fileName_poses) {
/*  PROGRAMMER: Frederick Wachter - wachterfreddy@gmail.com
    DATE CREATED: 2016-06-27
    PURPOSE: Execute logging for joint and pose locations

    INPUT(S):
      > right_arm - right arm move group
      > left_arm - left arm move group
      > command - string signifying type of point
      > output_fileName - full path of the output file
    OUTPUT(S): None
*/
    // Execute Point and Joint Writer Functions
    writeJoints(group1, group2, command, fileName_joints); // store the joints values of both arms
    writePose(group1, group2, command, fileName_poses); // store the the pose of both arms
}

