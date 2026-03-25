#pragma once

#include <nav_msgs/msg/occupancy_grid.hpp>
#include <rclcpp/rclcpp.hpp>
#include <string>

// MapPublisher — one-shot publisher of nav_msgs/OccupancyGrid to /map.
// Intra-process compatible: create with NodeOptions().use_intra_process_comms(true).
class MapPublisher : public rclcpp::Node {
public:
    MapPublisher(const std::string& map_path,
                 const rclcpp::NodeOptions& opts);

private:
    void publish_map();

    std::string map_path_;
    rclcpp::Publisher<nav_msgs::msg::OccupancyGrid>::SharedPtr pub_;
    rclcpp::TimerBase::SharedPtr timer_;
};
