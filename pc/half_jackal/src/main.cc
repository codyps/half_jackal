#include <ros/ros.h>
#include <tf/transform_broadcaster.h>
#include <nav_msgs/Odometry.h>

#include <sstream>

#include <boost/asio.hpp>
#include <boost/thread.hpp>

#include <frame_async.h>

#include <termios.h>
#include <unistd.h>

static int serial_conf(int fd, speed_t speed)
{
	struct termios t;
	int ret = tcgetattr(fd, &t);

	if (ret < 0)
		return ret;

	ret = cfsetispeed(&t, speed);
	if (ret < 0)
		return ret;

	ret = cfsetospeed(&t, speed);
	if (ret < 0)
		return ret;


	t.c_cflag |= PARENB | PARODD;

	return tcsetattr(fd, TCSANOW, &t);
}

static void recv_thread(FILE *sf)
{
	/* TODO: this is presently example code. */
	double x = 0.0;
	double y = 0.0;
	double th = 0.0;

	double vx = 0.1;
	double vy = -0.1;
	double vth = 0.1;

	ros::Time current_time, last_time;
	current_time = ros::Time::now();
	last_time = ros::Time::now();

	ros::Rate r(1.0);
	while(n.ok()){
		current_time = ros::Time::now();

		//compute odometry in a typical way given the velocities of the robot
		double dt = (current_time - last_time).toSec();
		double delta_x = (vx * cos(th) - vy * sin(th)) * dt;
		double delta_y = (vx * sin(th) + vy * cos(th)) * dt;
		double delta_th = vth * dt;

		x += delta_x;
		y += delta_y;
		th += delta_th;

		//since all odometry is 6DOF we'll need a quaternion created from yaw
		geometry_msgs::Quaternion odom_quat = tf::createQuaternionMsgFromYaw(th);

		//first, we'll publish the transform over tf
		geometry_msgs::TransformStamped odom_trans;
		odom_trans.header.stamp = current_time;
		odom_trans.header.frame_id = "odom";
		odom_trans.child_frame_id = "base_link";

		odom_trans.transform.translation.x = x;
		odom_trans.transform.translation.y = y;
		odom_trans.transform.translation.z = 0.0;
		odom_trans.transform.rotation = odom_quat;

		//send the transform
		odom_broadcaster.sendTransform(odom_trans);

		//next, we'll publish the odometry message over ROS
		nav_msgs::Odometry odom;
		odom.header.stamp = current_time;
		odom.header.frame_id = "odom";

		//set the position
		odom.pose.pose.position.x = x;
		odom.pose.pose.position.y = y;
		odom.pose.pose.position.z = 0.0;
		odom.pose.pose.orientation = odom_quat;

		//set the velocity
		odom.child_frame_id = "base_link";
		odom.twist.twist.linear.x = vx;
		odom.twist.twist.linear.y = vy;
		odom.twist.twist.angular.z = vth;

		//publish the message
		odom_pub.publish(odom);

		last_time = current_time;
		r.sleep();
	}
}

void compute_motors(struct hjb_pkt_set_speed *ss,
		const geometry_msgs::Twist::ConstPtr& msg)
{
	/* TODO: impliment */
}

/* direction_sub - subscriber callback for a direction message */
static void direction_sub(const geometry_msgs::Twist::ConstPtr& msg, FILE *sf)
{
	struct hjb_pkt_set_speed ss;
	compute_motors(&ss, msg);
	frame_send(sf, &ss, sizeof(ss));
}

int main(int argc, char **argv)
{
	ros::init(argc, argv, "half_jackal");

	ros::NodeHandle n;
	ros::NodeHandle n_priv("~");

	std::string serial_port
	if (!n_priv.getParam("serial_port", serial_port)) {
		ROS_ERROR("no serial port specified for param \"serial_port\"");
		return -1;
	}

	int sfd = open(serial_port, O_RDWR);
	if (sfd < 0) {
		ROS_ERROR("open: %s: %s", serial_port, strerror(errno));
		return -1;
	}

	int ret = serial_conf(sfd, B57600);
	if (ret < 0) {
		ROS_ERROR("serial_conf: %s: %s", serial_port, strerror(errno));
		return -1;
	}

	ros::Publisher a_current = n.advertise<std_msgs::Int>("a/current", 50);
	ros::Publisher b_current = n.advertise<std_msgs::Int>("b/current", 50);

	ros::Publisher odom = n.advertise<nav_msgs::Odometry>("odom", 50);
	tf::TransformBroadcaster odom_broadcaster;


	FILE *sf = fdopen(sfd, "a+")
	if (!sf) {
		ROS_ERROR("fdopen: %s: %s", serial_port, strerror(errno));
		return -1;
	}

	ros::Subscriber n.subscribe("direction", 1, direction_sub, sf);

	boost::thread recv_th(recv_thread, sf);

	ros::spin();

	return 0;
}
