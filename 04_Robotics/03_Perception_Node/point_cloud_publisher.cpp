// point_cloud_publisher.cpp — C3 Perception Node (Applied OpenCL Lab)
//
// Composable synthetic PointCloud2 publisher for reproducible benchmarking
// and visual validation.  Parameters replace former CLI11 flags:
//
//   topic   — topic name to publish on     (default: /points)
//   hz      — publish rate in Hz           (default: 200)
//   points  — points per message           (default: 100000)
//   scene   — grid | mixed                 (default: grid)
//
// Scene types:
//   grid   — uniform XYZ grid, z∈[0.5,5.4] m, intensity∈[50,249]. All points
//             pass the default filter. Use for throughput benchmarking.
//   mixed  — design-doc validation scene (§C3):
//             · 3 valid clusters at (2,0,1),(−2,0,1),(0,3,1) r=0.3 m intensity=150
//             · ground band z∈[−0.1, 0.05] m intensity=200  → filtered by ground_z=0.1
//             · low-intensity blob at (0,0,2) intensity=10   → filtered by min_intensity=50
//             Launch perception_node with ground_z:=0.1 min_intensity:=50

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_components/register_node_macro.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/point_field.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using PointCloud2 = sensor_msgs::msg::PointCloud2;
using PointField  = sensor_msgs::msg::PointField;

namespace perception_node {

// ─────────────────────────────────────────────────────────────────────────────
// PointCloudPublisher
// ─────────────────────────────────────────────────────────────────────────────
class PointCloudPublisher : public rclcpp::Node {
public:
    explicit PointCloudPublisher(const rclcpp::NodeOptions& opts)
        : rclcpp::Node("point_cloud_publisher", opts)
    {
        declare_parameter("topic",  std::string("/points"));
        declare_parameter("hz",     200);
        declare_parameter("points", 100000);
        declare_parameter("scene",  std::string("grid"));

        const std::string topic  = get_parameter("topic").as_string();
        const int         hz     = static_cast<int>(get_parameter("hz").as_int());
        num_points_              = static_cast<int>(get_parameter("points").as_int());
        const std::string scene  = get_parameter("scene").as_string();

        pub_ = create_publisher<PointCloud2>(topic, 10);

        // Pre-build the static cloud data once to avoid per-message allocation.
        if (scene == "mixed") {
            build_mixed_scene();
        } else {
            build_cloud_data();
        }

        const auto period_ms = std::chrono::milliseconds(1000 / hz);
        timer_ = create_wall_timer(period_ms, [this]() { publish_once(); });

        RCLCPP_INFO(get_logger(),
            "Publishing %d points at %d Hz on %s (scene: %s)",
            num_points_, hz, topic.c_str(), scene.c_str());
    }

private:
    // Build synthetic XYZ + intensity float32 data: grid layout, z in [0, 5] m,
    // intensity in [0, 255]. Layout: x(f32), y(f32), z(f32), intensity(f32) =
    // 16 bytes per point (point_step = 16).
    void build_cloud_data()
    {
        // §7.1: promote num_points_ before multiply to avoid signed overflow.
        const size_t total_floats = static_cast<size_t>(num_points_) * 4;
        cloud_floats_.resize(total_floats);

        const int side = static_cast<int>(std::sqrt(static_cast<double>(num_points_))) + 1;
        const float spacing = 0.05f;

        for (int i = 0; i < num_points_; ++i) {
            int row = i / side;
            int col = i % side;
            size_t base = static_cast<size_t>(i) * 4;
            cloud_floats_[base + 0] = static_cast<float>(col) * spacing;
            cloud_floats_[base + 1] = static_cast<float>(row) * spacing;
            cloud_floats_[base + 2] = 0.5f + 0.1f * (i % 50);
            cloud_floats_[base + 3] = 50.0f + static_cast<float>(i % 200);
        }
    }

    // Mixed scene matching design doc §C3 visual validation.
    // WHY deterministic (no std::rand): same output every frame → stable RViz display.
    void build_mixed_scene()
    {
        const size_t total_floats = static_cast<size_t>(num_points_) * 4;
        cloud_floats_.resize(total_floats);

        const int cluster_pts_each = num_points_ / 10;
        const int cluster_total    = cluster_pts_each * 3;
        const int ground_pts       = num_points_ * 4 / 10;
        const int noise_pts        = num_points_ - cluster_total - ground_pts;

        const float cx[3] = { 2.0f, -2.0f, 0.0f };
        const float cy[3] = { 0.0f,  0.0f, 3.0f };
        const float cz[3] = { 1.0f,  1.0f, 1.0f };
        const float R      = 0.3f;

        int idx = 0;

        for (int c = 0; c < 3; ++c) {
            for (int i = 0; i < cluster_pts_each; ++i) {
                float t     = static_cast<float>(i) / static_cast<float>(cluster_pts_each);
                float phi   = t * static_cast<float>(M_PI);
                float theta = t * 2.0f * static_cast<float>(M_PI) * 7.0f;
                float r     = R * (0.3f + 0.7f * t);
                size_t base = static_cast<size_t>(idx) * 4;
                cloud_floats_[base + 0] = cx[c] + r * std::sin(phi) * std::cos(theta);
                cloud_floats_[base + 1] = cy[c] + r * std::sin(phi) * std::sin(theta);
                cloud_floats_[base + 2] = cz[c] + r * std::cos(phi);
                cloud_floats_[base + 3] = 150.0f;
                ++idx;
            }
        }

        {
            const int   side    = static_cast<int>(std::sqrt(static_cast<double>(ground_pts))) + 1;
            const float spacing = 0.1f;
            const float offset  = static_cast<float>(side) * spacing * 0.5f;
            for (int i = 0; i < ground_pts; ++i) {
                float t = static_cast<float>(i) / static_cast<float>(ground_pts);
                size_t base = static_cast<size_t>(idx) * 4;
                cloud_floats_[base + 0] = static_cast<float>(i % side) * spacing - offset;
                cloud_floats_[base + 1] = static_cast<float>(i / side) * spacing - offset;
                cloud_floats_[base + 2] = -0.1f + t * 0.15f;
                cloud_floats_[base + 3] = 200.0f;
                ++idx;
            }
        }

        for (int i = 0; i < noise_pts; ++i) {
            float t     = static_cast<float>(i) / static_cast<float>(noise_pts);
            float phi   = t * static_cast<float>(M_PI);
            float theta = t * 2.0f * static_cast<float>(M_PI) * 13.0f;
            float r     = 0.5f * t;
            size_t base = static_cast<size_t>(idx) * 4;
            cloud_floats_[base + 0] = r * std::sin(phi) * std::cos(theta);
            cloud_floats_[base + 1] = r * std::sin(phi) * std::sin(theta);
            cloud_floats_[base + 2] = 2.0f + r * std::cos(phi);
            cloud_floats_[base + 3] = 10.0f;
            ++idx;
        }
    }

    void publish_once()
    {
        auto msg         = std::make_unique<PointCloud2>();
        msg->header.stamp = now();
        msg->header.frame_id = "lidar_link";
        msg->height     = 1;
        msg->width      = static_cast<uint32_t>(num_points_);
        msg->point_step = 16;
        msg->row_step   = msg->point_step * msg->width;
        msg->is_dense   = true;

        PointField fx, fy, fz, fi;
        fx.name = "x";         fx.offset = 0;  fx.datatype = PointField::FLOAT32; fx.count = 1;
        fy.name = "y";         fy.offset = 4;  fy.datatype = PointField::FLOAT32; fy.count = 1;
        fz.name = "z";         fz.offset = 8;  fz.datatype = PointField::FLOAT32; fz.count = 1;
        fi.name = "intensity"; fi.offset = 12; fi.datatype = PointField::FLOAT32; fi.count = 1;
        msg->fields = {fx, fy, fz, fi};

        const size_t byte_count = static_cast<size_t>(num_points_) * 16;
        msg->data.resize(byte_count);
        std::memcpy(msg->data.data(), cloud_floats_.data(), byte_count);

        pub_->publish(std::move(msg));
    }

    int num_points_;
    std::vector<float> cloud_floats_;
    rclcpp::Publisher<PointCloud2>::SharedPtr pub_;
    rclcpp::TimerBase::SharedPtr timer_;
};

}  // namespace perception_node

RCLCPP_COMPONENTS_REGISTER_NODE(perception_node::PointCloudPublisher)
