#pragma once

#include <nav_msgs/msg/occupancy_grid.hpp>
#include <rclcpp/rclcpp.hpp>
#include <string>

namespace costmap_inflation {

// MapPublisher — one-shot publisher of nav_msgs/OccupancyGrid to /map.
// map_path parameter (default "": generates a synthetic 512×512 grid).
// Intra-process compatible: use_intra_process_comms via NodeOptions.
class MapPublisher : public rclcpp::Node {
public:
    explicit MapPublisher(const rclcpp::NodeOptions& opts);

private:
    void publish_map();

    std::string map_path_;
    rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr pub_;
    rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace costmap_inflation
