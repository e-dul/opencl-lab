// map_publisher.cpp — MapPublisher node for C2 Costmap Inflation
//
// Publishes a nav_msgs/OccupancyGrid to /map once, on a 10 ms one-shot timer.
// When map_path is empty, a 512×512 synthetic grid is generated in memory —
// this keeps the binary runnable without any asset files.

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>

#include "map_publisher.hpp"

#include <rclcpp_components/register_node_macro.hpp>

#include <nav_msgs/msg/occupancy_grid.hpp>
#include <rclcpp/rclcpp.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace costmap_inflation {

// ─────────────────────────────────────────────────────────────────────────────
// generate_synthetic_grid
// ─────────────────────────────────────────────────────────────────────────────
// Creates a 512×512 occupancy grid in memory:
//   border walls + 4 rectangular obstacle clusters.
// Values: 100 = obstacle, 0 = free  (ROS nav_msgs convention).
static std::vector<int8_t> generate_synthetic_grid(int width, int height)
{
    std::vector<int8_t> grid(static_cast<size_t>(width) * height, 0);

    auto fill_rect = [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y <= y1; ++y)
            for (int x = x0; x <= x1; ++x)
                grid[static_cast<size_t>(y) * width + x] = 100;
    };

    // Border walls (10 px thick)
    const int T = 10;
    fill_rect(0, 0, width - 1, T - 1);
    fill_rect(0, height - T, width - 1, height - 1);
    fill_rect(0, 0, T - 1, height - 1);
    fill_rect(width - T, 0, width - 1, height - 1);

    // Interior obstacle clusters (mirroring gen_pgm.py layout)
    fill_rect(static_cast<int>(0.20 * width), static_cast<int>(0.20 * height),
              static_cast<int>(0.35 * width), static_cast<int>(0.40 * height));
    fill_rect(static_cast<int>(0.60 * width), static_cast<int>(0.15 * height),
              static_cast<int>(0.75 * width), static_cast<int>(0.35 * height));
    fill_rect(static_cast<int>(0.25 * width), static_cast<int>(0.60 * height),
              static_cast<int>(0.45 * width), static_cast<int>(0.75 * height));
    fill_rect(static_cast<int>(0.55 * width), static_cast<int>(0.55 * height),
              static_cast<int>(0.80 * width), static_cast<int>(0.70 * height));

    return grid;
}

// ─────────────────────────────────────────────────────────────────────────────
// MapPublisher implementation
// ─────────────────────────────────────────────────────────────────────────────

MapPublisher::MapPublisher(const rclcpp::NodeOptions& opts)
    : rclcpp::Node("map_publisher", opts)
{
    declare_parameter("map_path", std::string(""));
    map_path_ = get_parameter("map_path").as_string();

    // WHY transient_local QoS: CostmapNode subscribes after MapPublisher has
    // published. transient_local acts as a "latching" publisher — late joiners
    // receive the last message even if they connect after it was sent.
    auto qos = rclcpp::QoS(1).transient_local();
    pub_ = create_publisher<nav_msgs::msg::OccupancyGrid>("/map", qos);

    // WHY 10 ms delay: gives CostmapNode's subscription (created in on_activate)
    // time to register before the message is published.
    timer_ = create_wall_timer(
        std::chrono::milliseconds(10),
        [this]() { publish_map(); });
}

void MapPublisher::publish_map()
{
    timer_->cancel();  // one-shot: fire exactly once

    auto msg = std::make_unique<nav_msgs::msg::OccupancyGrid>();
    msg->header.stamp    = now();
    msg->header.frame_id = "map";

    if (map_path_.empty()) {
        constexpr int W = 512;
        constexpr int H = 512;
        msg->info.width      = W;
        msg->info.height     = H;
        msg->info.resolution = 0.05f;
        msg->data            = generate_synthetic_grid(W, H);
        RCLCPP_INFO(get_logger(), "[MAP] Published synthetic 512×512 grid.");
    } else {
        int w = 0, h = 0, ch = 0;
        uint8_t* raw = stbi_load(map_path_.c_str(), &w, &h, &ch, 1);
        if (!raw) {
            throw std::runtime_error("MapPublisher: stbi_load failed for '" +
                                     map_path_ + "': " + stbi_failure_reason());
        }
        const size_t n = static_cast<size_t>(w) * h;
        msg->info.width      = static_cast<uint32_t>(w);
        msg->info.height     = static_cast<uint32_t>(h);
        msg->info.resolution = 0.05f;
        msg->data.resize(n);
        for (size_t i = 0; i < n; ++i) {
            msg->data[i] = (raw[i] < 128) ? int8_t(100) : int8_t(0);
        }
        stbi_image_free(raw);
        RCLCPP_INFO(get_logger(), "[MAP] Published grid from '%s' (%dx%d).",
                    map_path_.c_str(), w, h);
    }

    pub_->publish(std::move(msg));
}

}  // namespace costmap_inflation

RCLCPP_COMPONENTS_REGISTER_NODE(costmap_inflation::MapPublisher)
