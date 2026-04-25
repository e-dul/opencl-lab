// synthetic_publisher.cpp — C1 Node Acceleration
//
// Composable node implementation for SyntheticPublisher.

#include "synthetic_publisher.hpp"

#include <rclcpp_components/register_node_macro.hpp>

using Float32MultiArray = std_msgs::msg::Float32MultiArray;

namespace node_acceleration {

SyntheticPublisher::SyntheticPublisher(const rclcpp::NodeOptions& opts)
    : rclcpp::Node("synthetic_publisher", opts)
{
    declare_parameter("iterations",  10);
    declare_parameter("buffer_size", 1'048'576);
    iterations_  = static_cast<int>(get_parameter("iterations").as_int());
    buffer_size_ = static_cast<int>(get_parameter("buffer_size").as_int());

    pub_ = create_publisher<Float32MultiArray>("/raw_floats", 10);

    // WHY 50 ms period: gives AccelNode's subscription callback time to process
    // each message before the next one arrives, avoiding queue buildup while
    // keeping the demo short.  AccelNode's auto_activate timer fires at 0ms,
    // so it is ACTIVE well before the first publish at 50ms.
    timer_ = create_wall_timer(
        std::chrono::milliseconds(50),
        [this]() { publish_once(); });
}

void SyntheticPublisher::publish_once()
{
    if (publish_count_ >= iterations_) {
        timer_->cancel();
        return;
    }

    // Pre-fill with 1.0f to ensure a real memory copy in the kernel.
    auto msg = std::make_unique<Float32MultiArray>();
    msg->data.assign(static_cast<size_t>(buffer_size_), 1.0f);
    pub_->publish(std::move(msg));
    ++publish_count_;
}

}  // namespace node_acceleration

RCLCPP_COMPONENTS_REGISTER_NODE(node_acceleration::SyntheticPublisher)
