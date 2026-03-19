// synthetic_publisher.hpp — C1 Node Acceleration
//
// Companion node that drives AccelNode by publishing Float32MultiArray messages
// to /raw_floats. Running in the same executor with intra-process comms enabled
// means the message is delivered as a shared_ptr — no DDS serialization.
//
// WHY separate file: design spec §Directory lists synthetic_publisher.cpp as a
// distinct compilation unit. Keeping the class here makes AccelNode (main.cpp)
// and the publisher independently readable.

#pragma once

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/float32_multi_array.hpp>

#include <chrono>
#include <memory>

class SyntheticPublisher : public rclcpp::Node {
public:
    SyntheticPublisher(int iterations, int buffer_size,
                       const rclcpp::NodeOptions& opts)
        : rclcpp::Node("synthetic_publisher", opts)
        , iterations_(iterations)
        , buffer_size_(buffer_size)
    {
        pub_ = create_publisher<std_msgs::msg::Float32MultiArray>("/raw_floats", 10);

        // WHY 50 ms period: gives the AccelNode subscription callback time to
        // process each message before the next one arrives, avoiding queue
        // buildup while keeping the demo short.
        timer_ = create_wall_timer(
            std::chrono::milliseconds(50),
            [this]() { publish_once(); });
    }

private:
    void publish_once();

    int iterations_;
    int buffer_size_;
    int publish_count_ = 0;
    rclcpp::Publisher<std_msgs::msg::Float32MultiArray>::SharedPtr pub_;
    rclcpp::TimerBase::SharedPtr timer_;
};
