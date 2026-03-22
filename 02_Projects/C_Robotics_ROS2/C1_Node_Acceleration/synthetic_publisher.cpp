// synthetic_publisher.cpp — C1 Node Acceleration
//
// Implementation for SyntheticPublisher. Compiled as a separate TU to match
// the design spec §Directory structure.

#include "synthetic_publisher.hpp"

// WHY local alias: Float32MultiArray is verbose; alias scoped to this TU so
// it does not pollute any includer's namespace (unlike a header-level using).
using Float32MultiArray = std_msgs::msg::Float32MultiArray;

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
