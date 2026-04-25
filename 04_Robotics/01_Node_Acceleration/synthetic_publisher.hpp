// synthetic_publisher.hpp — C1 Node Acceleration
//
// Companion composable node that drives AccelNode by publishing
// Float32MultiArray messages to /raw_floats.  Running in the same
// ComposableNodeContainer with use_intra_process_comms=true means the message
// is delivered as a shared_ptr — no DDS serialization.
//
// WHY separate file: design spec §Directory lists synthetic_publisher.cpp as a
// distinct compilation unit. Keeping the class here makes AccelNode (main.cpp)
// and the publisher independently readable.

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/float32_multi_array.hpp>

#include <chrono>
#include <memory>

namespace node_acceleration {

class SyntheticPublisher : public rclcpp::Node {
public:
    explicit SyntheticPublisher(const rclcpp::NodeOptions& opts);

private:
    void publish_once();

    int iterations_;
    int buffer_size_;
    int publish_count_ = 0;
    rclcpp::Publisher<std_msgs::msg::Float32MultiArray>::SharedPtr pub_;
    rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace node_acceleration
