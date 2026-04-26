// main.cpp — 4.5 Voxel Mapping
//
// Subscribes to sensor_msgs/PointCloud2, accumulates a 3D voxel occupancy
// grid across frames using a GPU DDA ray-casting kernel, optionally filters
// dynamic objects via flip-count detection, and writes a top-down 2D BMP
// slice on shutdown.
//
// Key constraints:
//  - Sensor pose is STATIC (no odometry): all frames accumulate in sensor frame.
//  - OpenCL cl::Event profiling is mandatory; report ms to 3 decimal places.
//  - Total pipeline must be < 5 ms @ 100k points; print [WARN] if exceeded.
//  - Parameters via --ros-args -p <name>:=<value> (ros2 param set / ros2 param list).

// stb must be defined before image_utils.hpp (see image_utils.hpp header comment).
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
#include "image_utils.hpp"

#include "opencl_utils.hpp"   // CL_CHECK, load_kernel_source, get_kernels_dir
#include "ocl_wrapper.hpp"    // create_context, OclContext

#include <diagnostic_updater/diagnostic_updater.hpp>
#include <rclcpp_components/register_node_macro.hpp>

#include <rcl_interfaces/msg/parameter_descriptor.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <sensor_msgs/msg/point_field.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using PointCloud2 = sensor_msgs::msg::PointCloud2;
namespace fs = std::filesystem;

namespace voxel_mapping {

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Extract CL event duration in milliseconds.
static double event_ms(const cl::Event& ev) {
    cl_ulong t0 = ev.getProfilingInfo<CL_PROFILING_COMMAND_START>();
    cl_ulong t1 = ev.getProfilingInfo<CL_PROFILING_COMMAND_END>();
    return static_cast<double>(t1 - t0) * 1e-6;
}

// ─────────────────────────────────────────────────────────────────────────────
// VoxelGrid metadata
// ─────────────────────────────────────────────────────────────────────────────
struct GridDims {
    int gx, gy, gz;
    float resolution;

    // §7.1: promote to size_t before multiplying to prevent 32-bit overflow.
    size_t total() const { return static_cast<size_t>(gx) * gy * gz; }
};

// ─────────────────────────────────────────────────────────────────────────────
// VoxelMappingNode
// ─────────────────────────────────────────────────────────────────────────────
class VoxelMappingNode : public rclcpp::Node {
public:
    explicit VoxelMappingNode(const rclcpp::NodeOptions& opts)
        : rclcpp::Node("voxel_mapping", opts)
        , first_message_(true)
    {
        // ── Declare parameters with descriptors ───────────────────────────────
        {
            rcl_interfaces::msg::ParameterDescriptor d;
            d.description = "PointCloud2 topic to subscribe to";
            declare_parameter("topic", std::string("/points"), d);
        }
        {
            rcl_interfaces::msg::ParameterDescriptor d;
            d.description = "Voxel size in metres";
            rcl_interfaces::msg::FloatingPointRange r;
            r.from_value = 0.01; r.to_value = 5.0; r.step = 0.0;
            d.floating_point_range.push_back(r);
            declare_parameter("resolution", 0.1, d);
        }
        {
            rcl_interfaces::msg::ParameterDescriptor d;
            d.description = "Output BMP file path for the top-down voxel slice";
            declare_parameter("output", std::string("output_voxel_slice.bmp"), d);
        }
        {
            rcl_interfaces::msg::ParameterDescriptor d;
            d.description = "Enable dynamic-object flip-count filter";
            declare_parameter("enable_flip_filter", false, d);
        }
        {
            rcl_interfaces::msg::ParameterDescriptor d;
            d.description = "Flip count threshold: voxels with count > N are erased";
            rcl_interfaces::msg::IntegerRange r;
            r.from_value = 1;
            r.to_value   = static_cast<int64_t>(std::numeric_limits<uint32_t>::max());
            r.step       = 0;
            d.integer_range.push_back(r);
            declare_parameter("flip_threshold", int64_t(5), d);
        }

        // Validation callback: reject flip_threshold > UINT32_MAX and resolution <= 0.
        param_cb_handle_ = add_on_set_parameters_callback(
            [this](const std::vector<rclcpp::Parameter>& params)
            -> rcl_interfaces::msg::SetParametersResult {
                rcl_interfaces::msg::SetParametersResult result;
                result.successful = true;
                for (const auto& p : params) {
                    if (p.get_name() == "flip_threshold" &&
                        p.as_int() > static_cast<int64_t>(std::numeric_limits<uint32_t>::max())) {
                        result.successful = false;
                        result.reason = "flip_threshold exceeds UINT32_MAX";
                        return result;
                    }
                    if (p.get_name() == "resolution" && p.as_double() <= 0.0) {
                        result.successful = false;
                        result.reason = "resolution must be > 0";
                        return result;
                    }
                }
                return result;
            });

        // ── Read params and assign to members ─────────────────────────────────
        topic_                     = get_parameter("topic").as_string();
        const float resolution     = static_cast<float>(get_parameter("resolution").as_double());
        resolution_                = resolution;
        output_path_               = get_parameter("output").as_string();
        enable_flip_               = get_parameter("enable_flip_filter").as_bool();
        const int64_t ft_val       = get_parameter("flip_threshold").as_int();
        flip_threshold_            = static_cast<uint32_t>(ft_val);

        // ── Grid dimensions: 20m × 20m × 5m ──────────────────────────────────
        // WHY these world extents: covers typical indoor/outdoor LiDAR range
        // while staying within fast GPU memory limits at 0.1m resolution.
        grid_.gx         = static_cast<int>(20.0f / resolution);
        grid_.gy         = static_cast<int>(20.0f / resolution);
        grid_.gz         = static_cast<int>( 5.0f / resolution);
        grid_.resolution = resolution;

        // ── OpenCL setup ──────────────────────────────────────────────────────
        ocl_ = create_context();

        // WHY CL_QUEUE_PROFILING_ENABLE: required to call getProfilingInfo on
        // cl::Event objects. Without this flag the timestamps return 0.
        cl_int err;
        profiling_queue_ = cl::CommandQueue(
            ocl_.context, ocl_.device, CL_QUEUE_PROFILING_ENABLE, &err);
        CL_CHECK(err);

        // ── Allocate voxel grid buffer (zero-initialised) ─────────────────────
        // §7.1: cast grid dims to size_t before multiplying.
        const size_t voxel_bytes =
            static_cast<size_t>(grid_.gx) * grid_.gy * grid_.gz * sizeof(cl_uint);

        grid_buf_ = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE, voxel_bytes);
        CL_CHECK(profiling_queue_.enqueueFillBuffer(
            grid_buf_, static_cast<cl_uint>(0), 0, voxel_bytes));

        if (enable_flip_) {
            prev_grid_buf_ = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE, voxel_bytes);
            flip_count_buf_ = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE, voxel_bytes);
            CL_CHECK(profiling_queue_.enqueueFillBuffer(
                prev_grid_buf_, static_cast<cl_uint>(0), 0, voxel_bytes));
            CL_CHECK(profiling_queue_.enqueueFillBuffer(
                flip_count_buf_, static_cast<cl_uint>(0), 0, voxel_bytes));
        }
        CL_CHECK(profiling_queue_.finish());

        // ── Build DDA kernel ──────────────────────────────────────────────────
        // WHY get_kernels_dir(): tries binary dir first (cmake standalone builds),
        // falls back to ament share directory when running as a composable node
        // (where /proc/self/exe is the component container, not the package binary).
        const fs::path kdir = get_kernels_dir("voxel_mapping");
        const std::string dda_src = load_kernel_source(
            (kdir / "dda_cast.cl").string());
        cl::Program::Sources dda_sources{{dda_src.c_str(), dda_src.size()}};
        dda_program_ = cl::Program(ocl_.context, dda_sources);
        dda_program_.build({ocl_.device});
        dda_kernel_ = cl::Kernel(dda_program_, "dda_cast");

        // ── Build flip-count + clear-occupied kernels (only when requested) ─────
        if (enable_flip_) {
            const std::string fc_src = load_kernel_source(
                (kdir / "flip_count.cl").string());
            cl::Program::Sources fc_sources{{fc_src.c_str(), fc_src.size()}};
            flip_program_ = cl::Program(ocl_.context, fc_sources);
            flip_program_.build({ocl_.device});
            flip_kernel_ = cl::Kernel(flip_program_, "count_flips");

            const std::string co_src = load_kernel_source(
                (kdir / "clear_occupied.cl").string());
            cl::Program::Sources co_sources{{co_src.c_str(), co_src.size()}};
            clear_occ_program_ = cl::Program(ocl_.context, co_sources);
            clear_occ_program_.build({ocl_.device});
            clear_occ_kernel_ = cl::Kernel(clear_occ_program_, "clear_occupied");
        }

        // ── ROS 2 subscriber ──────────────────────────────────────────────────
        // SensorDataQoS: BestEffort+Volatile matches sensor drivers; Reliable sub ↔ BestEffort pub = silent no-connection
        // Ref: https://docs.ros.org/en/jazzy/Concepts/Intermediate/About-Quality-of-Service-Settings.html
        sub_ = create_subscription<PointCloud2>(
            topic_, rclcpp::SensorDataQoS(),
            [this](PointCloud2::SharedPtr msg) { on_cloud(msg); });

        // KeepLast(1) Reliable: computed output; consumers (RViz2, rosbag2) expect reliable delivery of the latest result
        // Ref: https://docs.ros.org/en/jazzy/Concepts/Intermediate/About-Quality-of-Service-Settings.html
        voxel_map_pub_   = create_publisher<sensor_msgs::msg::PointCloud2>("/voxel_map",   rclcpp::QoS(rclcpp::KeepLast(1)));
        // KeepLast(1) Reliable: computed output; consumers (RViz2, rosbag2) expect reliable delivery of the latest result
        // Ref: https://docs.ros.org/en/jazzy/Concepts/Intermediate/About-Quality-of-Service-Settings.html
        voxel_slice_pub_ = create_publisher<sensor_msgs::msg::Image>("/voxel_slice", rclcpp::QoS(rclcpp::KeepLast(1)));

        RCLCPP_INFO(get_logger(),
            "Voxel grid: %dx%dx%d @ %.2fm resolution (%zu MB)",
            grid_.gx, grid_.gy, grid_.gz, resolution,
            voxel_bytes / (1024u * 1024u));

        // WHY unique_ptr (not direct member): initialising Updater in the
        // member initializer list registers its timer before the executor picks
        // up the node, so the callback group is never discovered. Constructing
        // it here, at the end of the constructor body, matches the pattern used
        // by C3 PerceptionNode (on_configure) and ensures the timer is live.
        // WHY no mutex: SingleThreadedExecutor serialises the subscription
        // callback (writes timing_buf_, msg_count_, points_in_acc_,
        // voxels_occupied_) and the diagnostic timer callback (reads them)
        // on the same thread — concurrent access is impossible without
        // changing the executor type.
        updater_ = std::make_unique<diagnostic_updater::Updater>(this);
        updater_->setHardwareID("none");
        updater_->add("GPU pipeline", this, &VoxelMappingNode::gpu_pipeline_diag);
        updater_->add("Config",       this, &VoxelMappingNode::config_diag);
        RCLCPP_INFO(get_logger(),
            "[DIAG] GPU timing and config published at 1 Hz — monitor with:\n"
            "       ros2 topic echo /diagnostics");
    }

    ~VoxelMappingNode()
    {
        // WHY destructor: composable nodes have no main() to call save_slice()
        // on Ctrl-C. The component container tears down nodes by destroying them,
        // so the destructor is the only reliable shutdown hook.
        try {
            save_slice();
        } catch (const std::exception& e) {
            // Log but do not rethrow — destructors must not propagate exceptions.
            RCLCPP_ERROR(get_logger(), "save_slice() failed during shutdown: %s", e.what());
        }
    }

    // Read back grid, write top-down BMP slice.
    void save_slice()
    {
        const size_t voxel_count =
            static_cast<size_t>(grid_.gx) * grid_.gy * grid_.gz;
        const size_t voxel_bytes = voxel_count * sizeof(cl_uint);

        // WHY no flip re-apply here: per-frame on_cloud() already writes the
        // filtered grid back to grid_buf_ after each callback. The grid read
        // here already reflects the last filtered state.
        std::vector<cl_uint> host_grid(voxel_count);
        CL_CHECK(profiling_queue_.enqueueReadBuffer(
            grid_buf_, CL_TRUE, 0, voxel_bytes, host_grid.data()));

        // Extract top-down 2D view via above-sensor column projection.
        // WHY projection instead of single-z slice: LiDAR returns span many
        // heights; a single slice misses data at other altitudes.
        // WHY above-sensor only (z >= gz/2): ground returns land at voxel z=gz/2-1
        // (just below sensor height). Including them causes the entire ground
        // footprint to appear as OCCUPIED and overwrites the FREE paths and sphere
        // OCCUPIED cells at higher z-levels. Projecting only z >= gz/2 shows the
        // above-sensor volume: sphere clusters appear as black dots, their ray
        // paths appear white, and unscanned areas remain grey.
        const int W  = grid_.gx;
        const int H  = grid_.gy;
        const int GZ = grid_.gz;
        // WHY gz/2+1: gz/2 is the sensor voxel itself. Ground points with z≈0
        // (top of the z∈[-0.1,0.05] band) land at voxel gz/2 and appear as a
        // spurious occupied rectangle in the projection. Starting one level above
        // the sensor excludes both ground and sensor-level voxels.
        const int z_start = GZ / 2 + 1;

        // Colour map: OCCUPIED=black(0), FREE=white(255), UNKNOWN=grey(128).
        std::vector<uint8_t> pixels(static_cast<size_t>(W) * H * 3);
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                cl_uint col_state = 0u;
                for (int z = z_start; z < GZ; ++z) {
                    size_t vi = static_cast<size_t>(x)
                              + static_cast<size_t>(y) * W
                              + static_cast<size_t>(z) * W * H;
                    col_state |= host_grid[vi];
                }

                uint8_t colour;
                if (col_state & 0x2u) {
                    colour = 0;    // OCCUPIED — black
                } else if (col_state & 0x1u) {
                    colour = 255;  // FREE (only) — white
                } else {
                    colour = 128;  // UNKNOWN — grey
                }

                size_t px = (static_cast<size_t>(y) * W + x) * 3;
                pixels[px + 0] = colour;
                pixels[px + 1] = colour;
                pixels[px + 2] = colour;
            }
        }

        save_bmp(output_path_, pixels, W, H, 3);
        std::cout << "[INFO] Wrote top-down voxel slice: " << output_path_
                  << " (" << W << "x" << H << ")\n";
    }

private:
    // Publishes debug topics: occupied voxel XYZ map and top-down MONO8 slice.
    // WHY separate from save_slice(): this is per-frame, save_slice() is on shutdown.
    // WHY independent readback: task spec requires no shared state between the two.
    void publish_debug(const std::vector<cl_uint>& host_grid,
                       const std_msgs::msg::Header& header)
    {
        using PointField = sensor_msgs::msg::PointField;
        const int GX = grid_.gx;
        const int GY = grid_.gy;
        const int GZ = grid_.gz;
        const float res = grid_.resolution;

        // ── /voxel_map: occupied voxel XYZ centroids ─────────────────────────
        // Collect all voxels with OCCUPIED_BIT set and convert to world XYZ.
        std::vector<float> pts_xyz;
        pts_xyz.reserve(1024 * 3);

        for (int vz = 0; vz < GZ; ++vz) {
            for (int vy = 0; vy < GY; ++vy) {
                for (int vx = 0; vx < GX; ++vx) {
                    size_t vi = static_cast<size_t>(vx)
                              + static_cast<size_t>(vy) * GX
                              + static_cast<size_t>(vz) * GX * GY;
                    if (host_grid[vi] & 0x2u) {
                        // WHY +0.5: voxel index is the lower-edge of the cell; the
                        // centroid is at the centre, shifted by half a voxel on each axis.
                        float wx = (static_cast<float>(vx - GX / 2) + 0.5f) * res;
                        float wy = (static_cast<float>(vy - GY / 2) + 0.5f) * res;
                        float wz = (static_cast<float>(vz - GZ / 2) + 0.5f) * res;
                        pts_xyz.push_back(wx);
                        pts_xyz.push_back(wy);
                        pts_xyz.push_back(wz);
                    }
                }
            }
        }

        const uint32_t n_occupied = static_cast<uint32_t>(pts_xyz.size() / 3);
        sensor_msgs::msg::PointCloud2 map_msg;
        map_msg.header     = header;
        map_msg.height     = 1;
        map_msg.width      = n_occupied;
        map_msg.is_dense   = true;
        map_msg.point_step = 12;  // 3 × float32
        map_msg.row_step   = map_msg.point_step * n_occupied;

        // Define XYZ fields with their byte offsets within each point.
        PointField fx; fx.name = "x"; fx.offset = 0;  fx.datatype = PointField::FLOAT32; fx.count = 1;
        PointField fy; fy.name = "y"; fy.offset = 4;  fy.datatype = PointField::FLOAT32; fy.count = 1;
        PointField fz; fz.name = "z"; fz.offset = 8;  fz.datatype = PointField::FLOAT32; fz.count = 1;
        map_msg.fields = {fx, fy, fz};

        map_msg.data.resize(static_cast<size_t>(n_occupied) * 12);
        if (n_occupied > 0) {
            std::memcpy(map_msg.data.data(), pts_xyz.data(),
                        static_cast<size_t>(n_occupied) * 12);
        }
        voxel_map_pub_->publish(map_msg);

        // ── /voxel_slice: above-sensor column projection → MONO8 image ───────
        // Project z from gz/2+1 to gz-1 — same logic as save_slice().
        // WHY gz/2+1: matches save_slice(); excludes sensor-level ground voxels.
        const int z_start = GZ / 2 + 1;

        sensor_msgs::msg::Image slice_msg;
        slice_msg.header   = header;
        slice_msg.width    = static_cast<uint32_t>(GX);
        slice_msg.height   = static_cast<uint32_t>(GY);
        slice_msg.encoding = "mono8";
        slice_msg.step     = static_cast<uint32_t>(GX);
        slice_msg.data.resize(static_cast<size_t>(GX) * GY);

        for (int y = 0; y < GY; ++y) {
            for (int x = 0; x < GX; ++x) {
                cl_uint col_state = 0u;
                for (int z = z_start; z < GZ; ++z) {
                    size_t vi = static_cast<size_t>(x)
                              + static_cast<size_t>(y) * GX
                              + static_cast<size_t>(z) * GX * GY;
                    col_state |= host_grid[vi];
                }
                uint8_t pixel;
                if (col_state & 0x2u) {
                    pixel = 0;    // OCCUPIED — black
                } else if (col_state & 0x1u) {
                    pixel = 255;  // FREE (only) — white
                } else {
                    pixel = 128;  // UNKNOWN — grey
                }
                slice_msg.data[static_cast<size_t>(y) * GX + x] = pixel;
            }
        }
        voxel_slice_pub_->publish(slice_msg);
    }

    void on_cloud(const PointCloud2::SharedPtr& msg)
    {
        if (first_message_) {
            first_message_ = false;
            std::cout << "[INFO] Sensor pose assumed static. "
                         "Odometry integration not implemented.\n";
            std::cout << "[INFO] Voxel grid: "
                      << grid_.gx << "x" << grid_.gy << "x" << grid_.gz
                      << " @ " << grid_.resolution << "m resolution\n";
        }

        // ── Parse XYZ floats from PointCloud2 byte buffer ─────────────────────
        // WHY parse manually: PointCloud2 stores raw bytes; field offsets and
        // point_step may vary by publisher — we read them from the message.
        const uint32_t point_step = msg->point_step;
        const uint32_t n_points   = msg->width * msg->height;

        if (n_points == 0) return;

        // Extract XYZ offsets from field descriptors.
        uint32_t off_x = 0, off_y = 4, off_z = 8;
        for (const auto& f : msg->fields) {
            if (f.name == "x") off_x = f.offset;
            else if (f.name == "y") off_y = f.offset;
            else if (f.name == "z") off_z = f.offset;
        }

        // Pack XYZ into a flat float array (stride=3) for the kernel.
        // WHY separate buffer: the kernel signature uses packed XYZ (no intensity).
        std::vector<float> xyz_pts(static_cast<size_t>(n_points) * 3);
        const uint8_t* raw = msg->data.data();
        for (uint32_t i = 0; i < n_points; ++i) {
            const uint8_t* p = raw + static_cast<size_t>(i) * point_step;
            float x, y, z;
            std::memcpy(&x, p + off_x, sizeof(float));
            std::memcpy(&y, p + off_y, sizeof(float));
            std::memcpy(&z, p + off_z, sizeof(float));
            size_t base = static_cast<size_t>(i) * 3;
            xyz_pts[base + 0] = x;
            xyz_pts[base + 1] = y;
            xyz_pts[base + 2] = z;
        }

        // ── Upload points to GPU ──────────────────────────────────────────────
        const size_t pts_bytes = static_cast<size_t>(n_points) * 3 * sizeof(float);
        // WHY enqueueWriteBuffer instead of CL_MEM_COPY_HOST_PTR: we need the
        // cl::Event handle to profile upload time; constructor flags do not
        // expose an event.
        cl::Buffer pts_buf2(ocl_.context, CL_MEM_READ_ONLY, pts_bytes);
        cl::Event upload_ev;
        CL_CHECK(profiling_queue_.enqueueWriteBuffer(
            pts_buf2, CL_FALSE, 0, pts_bytes, xyz_pts.data(),
            nullptr, &upload_ev));

        // ── Snapshot previous grid for flip-count kernel ───────────────────────
        if (enable_flip_) {
            const size_t vb = static_cast<size_t>(grid_.gx) * grid_.gy * grid_.gz * sizeof(cl_uint);
            CL_CHECK(profiling_queue_.enqueueCopyBuffer(grid_buf_, prev_grid_buf_, 0, 0, vb));
            // WHY clear only OCCUPIED_BIT (not full grid): atomic_or never un-sets
            // bits, so prev_grid ≈ curr_grid every frame and flip counts stagnate.
            // Clearing only OCCUPIED_BIT before DDA gives true per-frame occupancy
            // while FREE_BIT continues to accumulate across frames (free-space map
            // builds up). Static objects stay occupied every frame (no flip); dynamic
            // objects vacate old voxels (prev=OCCUPIED, curr=0 → flip) and appear at
            // new ones (prev=0, curr=OCCUPIED → flip), accumulating toward threshold.
            size_t total_sz = static_cast<size_t>(grid_.gx) * grid_.gy * grid_.gz;
            if (total_sz > static_cast<size_t>(INT_MAX))
                throw std::runtime_error("Grid too large for cl_int");
            cl_int total_voxels_co = static_cast<cl_int>(total_sz);
            CL_CHECK(clear_occ_kernel_.setArg(0, grid_buf_));
            CL_CHECK(clear_occ_kernel_.setArg(1, total_voxels_co));
            CL_CHECK(profiling_queue_.enqueueNDRangeKernel(
                clear_occ_kernel_,
                cl::NullRange,
                cl::NDRange(total_sz),
                cl::NullRange));
        }

        // ── DDA kernel dispatch ───────────────────────────────────────────────
        // Pack num_points into grid_dims.w (int4 .w field) as the kernel reads it there.
        cl_int4 gd;
        gd.s[0] = grid_.gx;
        gd.s[1] = grid_.gy;
        gd.s[2] = grid_.gz;
        gd.s[3] = static_cast<cl_int>(n_points);

        cl_float3 origin;
        origin.s[0] = 0.0f;
        origin.s[1] = 0.0f;
        origin.s[2] = 0.0f;

        CL_CHECK(dda_kernel_.setArg(0, pts_buf2));
        CL_CHECK(dda_kernel_.setArg(1, grid_buf_));
        CL_CHECK(dda_kernel_.setArg(2, gd));
        CL_CHECK(dda_kernel_.setArg(3, grid_.resolution));
        CL_CHECK(dda_kernel_.setArg(4, origin));

        cl::Event dda_ev;
        CL_CHECK(profiling_queue_.enqueueNDRangeKernel(
            dda_kernel_,
            cl::NullRange,
            cl::NDRange(static_cast<size_t>(n_points)),
            cl::NullRange,
            nullptr, &dda_ev));

        // ── Flip-count kernel dispatch (optional) ─────────────────────────────
        cl::Event flip_ev;
        double flip_ms = 0.0;

        if (enable_flip_) {
            // Must wait for DDA to complete before reading curr_grid.
            CL_CHECK(profiling_queue_.finish());

            // §7.1: promote to size_t before multiplying, then guard against cl_int overflow.
            size_t total_sz = static_cast<size_t>(grid_.gx) * grid_.gy * grid_.gz;
            if (total_sz > static_cast<size_t>(INT_MAX))
                throw std::runtime_error("Grid too large for cl_int");
            cl_int total_voxels = static_cast<cl_int>(total_sz);

            CL_CHECK(flip_kernel_.setArg(0, prev_grid_buf_));
            CL_CHECK(flip_kernel_.setArg(1, grid_buf_));
            CL_CHECK(flip_kernel_.setArg(2, flip_count_buf_));
            CL_CHECK(flip_kernel_.setArg(3, total_voxels));

            CL_CHECK(profiling_queue_.enqueueNDRangeKernel(
                flip_kernel_,
                cl::NullRange,
                cl::NDRange(static_cast<size_t>(total_voxels)),
                cl::NullRange,
                nullptr, &flip_ev));
        }

        CL_CHECK(profiling_queue_.finish());

        // ── Timing report ─────────────────────────────────────────────────────
        double upload_ms = event_ms(upload_ev);
        double dda_ms    = event_ms(dda_ev);
        if (enable_flip_) flip_ms = event_ms(flip_ev);
        double total_ms  = upload_ms + dda_ms + flip_ms;

        ++msg_count_;
        timing_buf_[timing_idx_++ % 100] = total_ms;
        ++timing_count_;
        points_in_acc_ += static_cast<double>(n_points);

        // ── Flip filter apply + debug readback (NOT in pipeline gate) ────────
        // WHY readback+writeback: the flip kernel only increments counters; the
        // actual zeroing of dynamic voxels must happen on the host (or a separate
        // zero kernel). Without writing the filtered grid back to grid_buf_, the
        // GPU grid accumulates dynamic-object voxels indefinitely and filtering
        // has no effect on subsequent frames.
        const size_t voxel_count = static_cast<size_t>(grid_.gx) * grid_.gy * grid_.gz;
        const size_t voxel_bytes = voxel_count * sizeof(cl_uint);

        std::vector<cl_uint> host_grid(voxel_count);
        CL_CHECK(profiling_queue_.enqueueReadBuffer(
            grid_buf_, CL_TRUE, 0, voxel_bytes, host_grid.data()));

        // Count occupied voxels (OCCUPIED_BIT = 0x2) for diagnostic reporting.
        uint64_t occ_count = 0;
        for (const auto& v : host_grid) {
            if (v & 0x2u) ++occ_count;
        }
        voxels_occupied_ = occ_count;

        if (enable_flip_) {
            std::vector<cl_uint> host_flips(voxel_count);
            CL_CHECK(profiling_queue_.enqueueReadBuffer(
                flip_count_buf_, CL_TRUE, 0, voxel_bytes, host_flips.data()));

            bool any_zeroed = false;
            for (size_t i = 0; i < voxel_count; ++i) {
                if (host_flips[i] > flip_threshold_) {
                    // WHY clear only OCCUPIED_BIT: preserves accumulated FREE_BIT
                    // so free-space information is not lost for dynamic-object paths.
                    host_grid[i]  &= ~static_cast<cl_uint>(0x2u);
                    // WHY reset flip count: without resetting, flip_count only
                    // grows and dynamic voxels are permanently filtered after the
                    // first threshold crossing. Resetting allows the voxel to
                    // re-accumulate flips next cycle so detection stays active
                    // as long as the object keeps moving.
                    host_flips[i]  = 0u;
                    any_zeroed     = true;
                }
            }

            // Write filtered grid and reset flip counts back to GPU so the next
            // frame starts from a clean per-cycle state.
            if (any_zeroed) {
                CL_CHECK(profiling_queue_.enqueueWriteBuffer(
                    grid_buf_, CL_TRUE, 0, voxel_bytes, host_grid.data()));
                CL_CHECK(profiling_queue_.enqueueWriteBuffer(
                    flip_count_buf_, CL_TRUE, 0, voxel_bytes, host_flips.data()));
            }
        }

        publish_debug(host_grid, msg->header);
    }

    // ── Diagnostic callbacks ──────────────────────────────────────────────────
    void gpu_pipeline_diag(diagnostic_updater::DiagnosticStatusWrapper& stat)
    {
        using DiagStatus = diagnostic_msgs::msg::DiagnosticStatus;
        const size_t n = std::min(timing_count_, size_t{100});
        if (n == 0) {
            stat.summary(DiagStatus::OK, "No data yet");
            return;
        }
        double sum = 0.0;
        double mn  = std::numeric_limits<double>::max();
        double mx  = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double v = timing_buf_[i];
            sum += v;
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        const double avg = sum / static_cast<double>(n);

        if (avg <= 5.0)
            stat.summary(DiagStatus::OK, "");
        else if (avg <= 15.0)
            stat.summary(DiagStatus::WARN, "Pipeline exceeds 5 ms gate");
        else
            stat.summary(DiagStatus::ERROR, "Pipeline stalled");

        stat.add("avg_kernel_ms",      avg);
        stat.add("min_kernel_ms",      mn);
        stat.add("max_kernel_ms",      mx);
        stat.add("messages_processed", msg_count_);
        if (msg_count_ > 0)
            stat.add("points_in_avg",
                     points_in_acc_ / static_cast<double>(msg_count_));
        stat.add("voxels_occupied",    voxels_occupied_);
        stat.add("total_voxels",       grid_.total());
    }

    void config_diag(diagnostic_updater::DiagnosticStatusWrapper& stat)
    {
        using DiagStatus = diagnostic_msgs::msg::DiagnosticStatus;
        stat.summary(DiagStatus::OK, "");
        stat.add("resolution",     resolution_);
        stat.add("flip_filter",    enable_flip_);
        stat.add("flip_threshold", flip_threshold_);
        stat.add("topic",          topic_);
    }

    // ── Members ───────────────────────────────────────────────────────────────
    OclContext   ocl_;
    cl::CommandQueue profiling_queue_;

    GridDims     grid_;
    cl::Buffer   grid_buf_;
    cl::Buffer   prev_grid_buf_;
    cl::Buffer   flip_count_buf_;

    cl::Program  dda_program_;
    cl::Kernel   dda_kernel_;
    cl::Program  flip_program_;
    cl::Kernel   flip_kernel_;
    cl::Program  clear_occ_program_;
    cl::Kernel   clear_occ_kernel_;

    std::string  output_path_;
    bool         enable_flip_;
    uint32_t     flip_threshold_;
    bool         first_message_;
    float        resolution_;
    std::string  topic_;

    // ── Diagnostic members ────────────────────────────────────────────────────
    std::unique_ptr<diagnostic_updater::Updater> updater_;
    std::array<double, 100> timing_buf_{};
    size_t   timing_idx_     = 0;
    size_t   timing_count_   = 0;
    uint64_t msg_count_      = 0;
    double   points_in_acc_  = 0.0;
    uint64_t voxels_occupied_= 0;

    // Param callback handle — must outlive the node.
    rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_cb_handle_;

    rclcpp::Publisher<sensor_msgs::msg::PointCloud2>::SharedPtr voxel_map_pub_;
    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr       voxel_slice_pub_;

    rclcpp::Subscription<PointCloud2>::SharedPtr sub_;
};

}  // namespace voxel_mapping

RCLCPP_COMPONENTS_REGISTER_NODE(voxel_mapping::VoxelMappingNode)
