// point_cloud_publisher.cpp — 4.5 Voxel Mapping synthetic publisher
//
// Generates deterministic PointCloud2 messages for testing voxel_mapping
// without real LiDAR hardware or ROS bag files.
//
// Scenes:
//   static  — sphere clusters at fixed positions; same every frame.
//             Use for DDA correctness checks (output_voxel_slice.bmp).
//   dynamic — same clusters, but their centres orbit (0,0,1) each frame.
//             Use with --enable-flip-filter to validate dynamic removal.
//
// WHY deterministic (no RNG): reproducible output across runs, stable RViz
// display, and compliant with the lab's "no std::rand" convention.
//
// Point layout: XYZI, point_step=16 (4 × float32). frame_id = "lidar_link".

#include <CLI/CLI.hpp>

#include <rclcpp/rclcpp.hpp>
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

// ─────────────────────────────────────────────────────────────────────────────
// Scene geometry constants (design spec §3 of the task)
// ─────────────────────────────────────────────────────────────────────────────
static constexpr float CLUSTER_R        = 0.3f;
static constexpr float CLUSTER_ORBIT_R  = 1.0f;
static constexpr float CLUSTER_BASE_Z   = 1.0f;
static constexpr float CLUSTER_INTENSITY = 150.0f;
static constexpr float GROUND_INTENSITY  = 200.0f;
static constexpr float NOISE_INTENSITY   = 10.0f;

// ─────────────────────────────────────────────────────────────────────────────
// VoxelCloudPublisher
// ─────────────────────────────────────────────────────────────────────────────
class VoxelCloudPublisher : public rclcpp::Node {
public:
    VoxelCloudPublisher(const std::string& topic, float hz,
                        uint32_t num_points, uint32_t max_frames,
                        const std::string& scene, float move_speed)
        : rclcpp::Node("voxel_point_cloud_publisher")
        , num_points_(static_cast<int>(num_points))
        , max_frames_(max_frames)
        , scene_(scene)
        , move_speed_(move_speed)
    {
        pub_ = create_publisher<PointCloud2>(topic, 10);

        // For static scenes, build the cloud data once to avoid per-callback
        // recomputation (the data never changes).
        if (scene_ == "static") {
            build_static_scene();
        }

        const auto period_ms = std::chrono::milliseconds(
            static_cast<int64_t>(1000.0f / hz));
        timer_ = create_wall_timer(period_ms, [this]() { on_timer(); });

        RCLCPP_INFO(get_logger(),
            "Publishing %d points at %.1f Hz on %s (scene: %s, max_frames: %u)",
            num_points_, hz, topic.c_str(), scene_.c_str(), max_frames_);
    }

private:
    void on_timer()
    {
        if (scene_ == "dynamic") {
            build_dynamic_scene(frame_index_);
        }

        publish_once();

        ++frame_index_;
        if (max_frames_ > 0 && frame_index_ >= max_frames_) {
            RCLCPP_INFO(get_logger(),
                "Published %u frames. Shutting down.", max_frames_);
            rclcpp::shutdown();
        }
    }

    // ── Static scene: fixed sphere clusters + ground + noise ─────────────────
    // Cluster centres at (2,0,1), (-2,0,1), (0,3,1), r=0.3 m, intensity=150.
    // Ground band: z∈[-0.1, 0.05], intensity=200.
    // Noise blob at (0,0,2), intensity=10.
    void build_static_scene()
    {
        static const float cx[3] = { 2.0f, -2.0f, 0.0f };
        static const float cy[3] = { 0.0f,  0.0f, 3.0f };
        static const float cz[3] = { 1.0f,  1.0f, 1.0f };
        fill_scene(cx, cy, cz);
    }

    // ── Dynamic scene: clusters orbit (0,0,1) at r=1 m per frame ─────────────
    // Angle advances by move_speed_ * frame_index each call.
    // Ground and noise remain at fixed positions.
    void build_dynamic_scene(uint32_t frame)
    {
        float angle = move_speed_ * static_cast<float>(frame);
        float cx[3], cy[3], cz[3];
        for (int c = 0; c < 3; ++c) {
            // Offset cluster phase by 2π/3 so the three clusters are spread out.
            float phase = angle + static_cast<float>(c) * (2.0f * static_cast<float>(M_PI) / 3.0f);
            cx[c] = CLUSTER_ORBIT_R * std::cos(phase);
            cy[c] = CLUSTER_ORBIT_R * std::sin(phase);
            cz[c] = CLUSTER_BASE_Z;
        }
        fill_scene(cx, cy, cz);
    }

    // Shared scene builder: fills cloud_floats_ with clusters + ground + noise.
    // Point budget: 30% clusters (10% each), 40% ground, 30% noise.
    void fill_scene(const float cx[3], const float cy[3], const float cz[3])
    {
        // §7.1: promote before multiply to avoid int32 overflow.
        cloud_floats_.resize(static_cast<size_t>(num_points_) * 4);

        const int cluster_pts_each = num_points_ / 10;
        const int cluster_total    = cluster_pts_each * 3;
        const int ground_pts       = num_points_ * 4 / 10;
        const int noise_pts        = num_points_ - cluster_total - ground_pts;

        int idx = 0;

        // 1. Sphere clusters — WHY spiral: fills volume deterministically without RNG.
        for (int c = 0; c < 3; ++c) {
            for (int i = 0; i < cluster_pts_each; ++i) {
                float t     = static_cast<float>(i) / static_cast<float>(cluster_pts_each);
                float phi   = t * static_cast<float>(M_PI);
                float theta = t * 2.0f * static_cast<float>(M_PI) * 7.0f;
                float r     = CLUSTER_R * (0.3f + 0.7f * t);
                size_t base = static_cast<size_t>(idx) * 4;
                cloud_floats_[base + 0] = cx[c] + r * std::sin(phi) * std::cos(theta);
                cloud_floats_[base + 1] = cy[c] + r * std::sin(phi) * std::sin(theta);
                cloud_floats_[base + 2] = cz[c] + r * std::cos(phi);
                cloud_floats_[base + 3] = CLUSTER_INTENSITY;
                ++idx;
            }
        }

        // 2. Ground band z∈[-0.1, 0.05], intensity=200.
        {
            const int   side    = static_cast<int>(std::sqrt(static_cast<double>(ground_pts))) + 1;
            const float spacing = 0.1f;
            const float offset  = static_cast<float>(side) * spacing * 0.5f;
            for (int i = 0; i < ground_pts; ++i) {
                // WHY row-based t: using the linear scan index creates a diagonal
                // z-gradient across the grid, which maps to two separate voxel
                // z-levels and appears as a tilted split plane in RViz. Parameterising
                // by row (y-grid index) gives a uniform slope along one axis only —
                // physically plausible ramp, no diagonal scan artefact.
                float t    = static_cast<float>(i / side) / static_cast<float>(side);
                size_t base = static_cast<size_t>(idx) * 4;
                cloud_floats_[base + 0] = static_cast<float>(i % side) * spacing - offset;
                cloud_floats_[base + 1] = static_cast<float>(i / side) * spacing - offset;
                cloud_floats_[base + 2] = -0.1f + t * 0.15f;   // z ∈ [-0.1, 0.05], ramps along y
                cloud_floats_[base + 3] = GROUND_INTENSITY;
                ++idx;
            }
        }

        // 3. Noise blob at (0,0,2), intensity=10.
        for (int i = 0; i < noise_pts; ++i) {
            float t     = static_cast<float>(i) / static_cast<float>(noise_pts);
            float phi   = t * static_cast<float>(M_PI);
            float theta = t * 2.0f * static_cast<float>(M_PI) * 13.0f;
            float r     = 0.5f * t;
            size_t base = static_cast<size_t>(idx) * 4;
            cloud_floats_[base + 0] = r * std::sin(phi) * std::cos(theta);
            cloud_floats_[base + 1] = r * std::sin(phi) * std::sin(theta);
            cloud_floats_[base + 2] = 2.0f + r * std::cos(phi);
            cloud_floats_[base + 3] = NOISE_INTENSITY;
            ++idx;
        }
    }

    void publish_once()
    {
        auto msg             = std::make_unique<PointCloud2>();
        msg->header.stamp    = now();
        msg->header.frame_id = "lidar_link";
        msg->height          = 1;
        msg->width           = static_cast<uint32_t>(num_points_);
        msg->point_step      = 16;  // 4 float32 fields × 4 bytes each
        msg->row_step        = msg->point_step * msg->width;
        msg->is_dense        = true;

        // Build field descriptors: XYZI layout.
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

    int                num_points_;
    uint32_t           max_frames_;
    std::string        scene_;
    float              move_speed_;
    uint32_t           frame_index_{0};

    std::vector<float> cloud_floats_;
    rclcpp::Publisher<PointCloud2>::SharedPtr  pub_;
    rclcpp::TimerBase::SharedPtr              timer_;
};

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    // Parse CLI before rclcpp::init so --help exits cleanly without ROS init.
    CLI::App app{"4.5 Voxel Mapping — Synthetic PointCloud2 Publisher"};

    std::string topic      = "/points";
    float       hz         = 10.0f;
    uint32_t    num_points = 10000;
    uint32_t    max_frames = 0;
    std::string scene      = "static";
    float       move_speed = 0.05f;

    app.add_option("--topic",      topic,      "PointCloud2 topic to publish on")->default_str(topic);
    app.add_option("--hz",         hz,         "Publish rate in Hz")->default_val(hz);
    app.add_option("--points",     num_points, "Points per message")->default_val(num_points);
    app.add_option("--frames",     max_frames, "Stop after N frames (0 = infinite)")->default_val(max_frames);
    app.add_option("--scene",      scene,
                   "Scene type: static | dynamic")
       ->default_str(scene)
       ->check(CLI::IsMember({"static", "dynamic"}));
    app.add_option("--move-speed", move_speed,
                   "Orbit angle increment per frame (rad, dynamic scene only)")->default_val(move_speed);

    CLI11_PARSE(app, argc, argv);

    rclcpp::init(argc, argv);

    auto node = std::make_shared<VoxelCloudPublisher>(
        topic, hz, num_points, max_frames, scene, move_speed);
    rclcpp::spin(node);

    rclcpp::shutdown();
    return 0;
}
