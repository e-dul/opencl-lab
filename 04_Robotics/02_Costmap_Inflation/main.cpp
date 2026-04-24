// main.cpp — C2 Costmap Inflation (Applied OpenCL Lab)
//
// Demonstrates:
//   1. CPU exact Euclidean distance transform as correctness oracle.
//   2. GPU naive inflate kernel (2D NDRange, global memory only).
//   3. GPU tiled inflate_tiled kernel (LDS tiled, Task 038).
//   4. Timing comparison table (CPU vs GPU naive vs GPU tiled).
//   5. BMP colorization: obstacles=black, inflated=red gradient, free=white.

#include "ocl_wrapper.hpp"    // create_context(), OclContext
#include "opencl_utils.hpp"   // CL_CHECK, build_program, duration_ms, get_binary_dir
#include "map_publisher.hpp"  // MapPublisher

#include <lifecycle_msgs/msg/state.hpp>
#include <lifecycle_msgs/msg/transition.hpp>
#include <nav_msgs/msg/occupancy_grid.hpp>
#include <rcl_interfaces/msg/parameter_descriptor.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>

// stb_image_write: implementation defined in map_publisher.cpp (STB_IMAGE_WRITE_IMPLEMENTATION)
// Include header here for stbi_write_bmp used in save_colorized_bmp()
#include <stb_image_write.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Clock           = std::chrono::steady_clock;
using CallbackReturn  = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;
using OccupancyGrid   = nav_msgs::msg::OccupancyGrid;

// ─────────────────────────────────────────────────────────────────────────────
// CostmapNode
// ─────────────────────────────────────────────────────────────────────────────
class CostmapNode : public rclcpp_lifecycle::LifecycleNode {
public:
    explicit CostmapNode(const rclcpp::NodeOptions& opts)
        : rclcpp_lifecycle::LifecycleNode("costmap_node", opts)
    {}

    // Expose map_path so main() can forward it to MapPublisher.
    const std::string& map_path() const { return map_path_; }

    // ── on_configure ─────────────────────────────────────────────────────────
    // Declare + read params, init OpenCL context, compile both kernels.
    CallbackReturn on_configure(const rclcpp_lifecycle::State&) override
    {
        {
            rcl_interfaces::msg::ParameterDescriptor d;
            d.description = "Path to .pgm occupancy grid file";
            declare_parameter("map_path", std::string(""), d);
        }
        {
            rcl_interfaces::msg::ParameterDescriptor d;
            d.description = "Obstacle inflation radius in metres";
            rcl_interfaces::msg::FloatingPointRange r;
            r.from_value = 0.01; r.to_value = 10.0; r.step = 0.0;
            d.floating_point_range.push_back(r);
            declare_parameter("inflation_radius", 0.5, d);
        }
        {
            rcl_interfaces::msg::ParameterDescriptor d;
            d.description = "Metres per cell";
            rcl_interfaces::msg::FloatingPointRange r;
            // Exclusive lower bound: smallest positive step value used as from_value
            // to convey "greater than 0"; the callback enforces > 0 explicitly.
            r.from_value = 0.001; r.to_value = 1.0; r.step = 0.0;
            d.floating_point_range.push_back(r);
            declare_parameter("resolution", 0.05, d);
        }
        {
            rcl_interfaces::msg::ParameterDescriptor d;
            d.description = "Exponential cost decay rate";
            rcl_interfaces::msg::FloatingPointRange r;
            r.from_value = 0.001; r.to_value = 100.0; r.step = 0.0;
            d.floating_point_range.push_back(r);
            declare_parameter("decay", 3.0, d);
        }

        map_path_        = get_parameter("map_path").as_string();
        inflation_radius_ = get_parameter("inflation_radius").as_double();
        resolution_       = get_parameter("resolution").as_double();
        decay_            = get_parameter("decay").as_double();

        // Validation callback: reject out-of-range values at runtime.
        // WHY register here (not constructor): member variables don't exist until
        // on_configure(); the callback reads them via get_parameter() so it is safe
        // to register at any point after declare_parameter.
        param_cb_handle_ = add_on_set_parameters_callback(
            [this](const std::vector<rclcpp::Parameter>& params)
            -> rcl_interfaces::msg::SetParametersResult {
                rcl_interfaces::msg::SetParametersResult result;
                result.successful = true;
                for (const auto& p : params) {
                    if (p.get_name() == "inflation_radius" && p.as_double() <= 0.0) {
                        result.successful = false;
                        result.reason = "inflation_radius must be > 0";
                        return result;
                    }
                    if (p.get_name() == "resolution" && p.as_double() <= 0.0) {
                        result.successful = false;
                        result.reason = "resolution must be > 0";
                        return result;
                    }
                    if (p.get_name() == "decay" && p.as_double() <= 0.0) {
                        result.successful = false;
                        result.reason = "decay must be > 0";
                        return result;
                    }
                }
                return result;
            });

        auto t0 = Clock::now();
        try {
            ocl_ = create_context();
        } catch (const std::exception& e) {
            RCLCPP_ERROR(get_logger(), "OpenCL init failed: %s", e.what());
            return CallbackReturn::FAILURE;
        }

        // WHY CL_QUEUE_PROFILING_ENABLE: required to read
        // CL_PROFILING_COMMAND_START / _END from cl::Event.
        cl_int err = CL_SUCCESS;
        queue_ = cl::CommandQueue(ocl_.context, ocl_.device,
                                  CL_QUEUE_PROFILING_ENABLE, &err);
        CL_CHECK(err);

        fs::path kdir = get_binary_dir() / "kernels";
        program_naive_  = build_program(ocl_.context, ocl_.device,
                                        (kdir / "inflate.cl").string());
        program_tiled_  = build_program(ocl_.context, ocl_.device,
                                        (kdir / "inflate_tiled.cl").string(),
                                        "-cl-std=CL1.2 -D TILE_W=" + std::to_string(TILE_W)
                                        + " -D TILE_H=" + std::to_string(TILE_H));

        inflate_kernel_       = cl::Kernel(program_naive_, "inflate");
        inflate_tiled_kernel_ = cl::Kernel(program_tiled_, "inflate_tiled");

        auto t1 = Clock::now();
        double init_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        RCLCPP_INFO(get_logger(), "[INIT] Context init: %.3f ms", init_ms);

        return CallbackReturn::SUCCESS;
    }

    // ── on_activate ──────────────────────────────────────────────────────────
    CallbackReturn on_activate(const rclcpp_lifecycle::State&) override
    {
        auto qos = rclcpp::QoS(1).transient_local();
        pub_ = create_publisher<OccupancyGrid>("/inflated_costmap", qos);
        pub_->on_activate();

        sub_ = create_subscription<OccupancyGrid>(
            "/map", qos,
            [this](OccupancyGrid::ConstSharedPtr msg) {
                on_map_received(msg);
            });

        return CallbackReturn::SUCCESS;
    }

    // ── on_deactivate ─────────────────────────────────────────────────────────
    CallbackReturn on_deactivate(const rclcpp_lifecycle::State&) override
    {
        sub_.reset();
        pub_->on_deactivate();
        return CallbackReturn::SUCCESS;
    }

    // ── on_cleanup ────────────────────────────────────────────────────────────
    // Release OpenCL resources via default-constructed assignment.
    // WHY explicit order (kernel → program → queue → context): avoids dangling
    // references from child objects outliving their parent context.
    CallbackReturn on_cleanup(const rclcpp_lifecycle::State&) override
    {
        inflate_kernel_       = cl::Kernel();
        inflate_tiled_kernel_ = cl::Kernel();
        program_naive_        = cl::Program();
        program_tiled_        = cl::Program();
        queue_                = cl::CommandQueue();
        ocl_                  = OclContext{};
        RCLCPP_INFO(get_logger(), "[DONE] OpenCL resources released.");
        return CallbackReturn::SUCCESS;
    }

private:
    // ── on_map_received ───────────────────────────────────────────────────────
    // Main processing callback: CPU DT → GPU naive → GPU tiled (LDS) →
    // correctness check → BMP save → publish → timing table → shutdown.
    void on_map_received(OccupancyGrid::ConstSharedPtr msg)
    {
        const int W = static_cast<int>(msg->info.width);
        const int H = static_cast<int>(msg->info.height);

        // §7.1: guard against integer overflow before buffer allocation.
        const size_t total = static_cast<size_t>(W) * H;
        if (total > static_cast<size_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("Grid too large: width*height exceeds INT_MAX");
        }

        // Build uchar obstacle map (1=obstacle, 0=free) from OccupancyGrid data.
        // OccupancyGrid convention: 100=obstacle, 0=free, -1=unknown (treat as free).
        std::vector<uint8_t> obstacle_map(total);
        for (size_t i = 0; i < total; ++i) {
            obstacle_map[i] = (msg->data[i] == 100) ? 1u : 0u;
        }

        const int radius_px = static_cast<int>(
            std::round(inflation_radius_ / resolution_));
        const float decay_f      = static_cast<float>(decay_);
        const float resolution_f = static_cast<float>(resolution_);

        // ── CPU exact Euclidean DT ────────────────────────────────────────────
        std::vector<uint8_t> cpu_result(total, 0);
        auto cpu_t0 = Clock::now();
        cpu_inflate(obstacle_map, cpu_result, W, H, radius_px, decay_f, resolution_f);
        auto cpu_t1 = Clock::now();
        double cpu_ms = std::chrono::duration<double, std::milli>(cpu_t1 - cpu_t0).count();

        // ── GPU naive inflate ─────────────────────────────────────────────────
        std::vector<uint8_t> gpu_result(total, 0);
        double gpu_naive_ms = run_gpu_kernel(
            inflate_kernel_, obstacle_map, gpu_result, W, H,
            radius_px, decay_f, resolution_f);

        // ── GPU tiled inflate ────────────────────────────────────────────────
        // TILE_W/TILE_H: class-level constants (also passed as -D macros to the
        // tiled kernel at compile time in on_configure()).
        std::vector<uint8_t> gpu_tiled_result(total, 0);
        double gpu_tiled_ms = -1.0;  // sentinel: -1.0 = skipped

        // Local memory guard: kernel needs a halo-padded tile in __local.
        size_t local_mem_needed =
            static_cast<size_t>(TILE_W + 2 * radius_px) *
            static_cast<size_t>(TILE_H + 2 * radius_px) *
            sizeof(cl_uchar);

        cl_ulong device_local_mem = 0;
        CL_CHECK(ocl_.device.getInfo(CL_DEVICE_LOCAL_MEM_SIZE, &device_local_mem));

        if (local_mem_needed > static_cast<size_t>(device_local_mem)) {
            RCLCPP_WARN(get_logger(),
                "[SKIP] tiled kernel: local mem %zu > device limit %llu bytes",
                local_mem_needed,
                static_cast<unsigned long long>(device_local_mem));
        } else {
            // Arg 7: __local uchar* tile — size set here so device allocates it.
            CL_CHECK(inflate_tiled_kernel_.setArg(7, cl::Local(local_mem_needed)));
            gpu_tiled_ms = run_gpu_kernel_tiled(
                inflate_tiled_kernel_, obstacle_map, gpu_tiled_result, W, H,
                radius_px, decay_f, resolution_f, TILE_W, TILE_H);
        }

        // ── Correctness check: GPU naive vs CPU reference ─────────────────────
        int max_dev = 0;
        int max_dev_x = 0, max_dev_y = 0;
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                size_t idx = static_cast<size_t>(y) * W + x;
                int dev = std::abs(static_cast<int>(gpu_result[idx]) -
                                   static_cast<int>(cpu_result[idx]));
                if (dev > max_dev) {
                    max_dev   = dev;
                    max_dev_x = x;
                    max_dev_y = y;
                }
            }
        }
        if (max_dev > 1) {
            RCLCPP_WARN(get_logger(),
                "[WARN] GPU naive vs CPU max deviation: %d cost units at (%d,%d)",
                max_dev, max_dev_x, max_dev_y);
        } else {
            RCLCPP_INFO(get_logger(),
                "[OK] GPU naive vs CPU max deviation: %d cost units", max_dev);
        }

        // ── Correctness check: GPU tiled vs CPU reference ─────────────────────
        if (gpu_tiled_ms >= 0.0) {
            int tiled_max_dev = 0;
            int tiled_dev_x = 0, tiled_dev_y = 0;
            for (int y = 0; y < H; ++y) {
                for (int x = 0; x < W; ++x) {
                    size_t idx = static_cast<size_t>(y) * W + x;
                    int dev = std::abs(static_cast<int>(gpu_tiled_result[idx]) -
                                       static_cast<int>(cpu_result[idx]));
                    if (dev > tiled_max_dev) {
                        tiled_max_dev = dev;
                        tiled_dev_x   = x;
                        tiled_dev_y   = y;
                    }
                }
            }
            if (tiled_max_dev > 1) {
                RCLCPP_WARN(get_logger(),
                    "[WARN] GPU tiled vs CPU max deviation: %d cost units at (%d,%d)",
                    tiled_max_dev, tiled_dev_x, tiled_dev_y);
            } else {
                RCLCPP_INFO(get_logger(),
                    "[OK] GPU tiled vs CPU max deviation: %d cost units", tiled_max_dev);
            }
        }

        // ── BMP colorization ──────────────────────────────────────────────────
        // obstacles→black, inflated→red gradient (R=255,G=0,B=0,A=cost), free→white
        save_colorized_bmp(obstacle_map, gpu_result, W, H);

        // ── Publish /inflated_costmap ─────────────────────────────────────────
        auto out_msg = std::make_unique<OccupancyGrid>();
        out_msg->header   = msg->header;
        out_msg->info     = msg->info;
        out_msg->data.resize(total);
        for (size_t i = 0; i < total; ++i) {
            // Convert 0-255 cost back to 0-100 OccupancyGrid scale.
            out_msg->data[i] = static_cast<int8_t>(
                static_cast<int>(gpu_result[i]) * 100 / 255);
        }
        pub_->publish(std::move(out_msg));

        // ── Timing table ──────────────────────────────────────────────────────
        double speedup_naive  = (gpu_naive_ms > 0.0) ? cpu_ms / gpu_naive_ms : 0.0;

        std::cout << std::fixed << std::setprecision(3)
            << "+-------------------------------------------+\n"
               "|  C2 Costmap Inflation                     |\n"
               "+------------------+------------------------+\n"
            << "| CPU DT           |" << std::setw(15) << cpu_ms       << " ms        |\n"
            << "| GPU naive        |" << std::setw(15) << gpu_naive_ms << " ms        |\n";

        if (gpu_tiled_ms < 0.0) {
            // Local mem guard fired — tiled kernel was skipped.
            std::cout << "| GPU tiled        |          0.000 ms [SKIP]          |\n"
                      << "| GPU-vs-CPU       |" << std::setw(15) << speedup_naive << "x speedup  |\n"
                      << "| Tiled-vs-naive   |                              N/A |\n"
                         "+------------------+------------------------+\n";
        } else {
            double speedup_tiled = (gpu_tiled_ms > 0.0) ? gpu_naive_ms / gpu_tiled_ms : 0.0;
            std::cout << "| GPU tiled        |" << std::setw(15) << gpu_tiled_ms << " ms        |\n"
                      << "| GPU-vs-CPU       |" << std::setw(15) << speedup_naive << "x speedup  |\n"
                      << "| Tiled-vs-naive   |" << std::setw(15) << speedup_tiled << "x speedup  |\n"
                         "+------------------+------------------------+\n";
        }

        // ── Self-shutdown via one-shot timer (avoids lifecycle mutex deadlock) ─
        // WHY timer (not direct call): trigger_transition from within a lifecycle
        // callback deadlocks because the state machine mutex is already held.
        shutdown_timer_ = create_wall_timer(
            std::chrono::milliseconds(0),
            [this]() {
                shutdown_timer_->cancel();

                trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_DEACTIVATE);
                if (get_current_state().id() !=
                        lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE) {
                    RCLCPP_ERROR(get_logger(),
                        "TRANSITION_DEACTIVATE did not reach INACTIVE");
                }

                trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_CLEANUP);
                if (get_current_state().id() !=
                        lifecycle_msgs::msg::State::PRIMARY_STATE_UNCONFIGURED) {
                    RCLCPP_ERROR(get_logger(),
                        "TRANSITION_CLEANUP did not reach UNCONFIGURED");
                }

                rclcpp::shutdown();
            });
    }

    // ── cpu_inflate ───────────────────────────────────────────────────────────
    // Exact Euclidean DT: for each free cell, scan [-radius_px, radius_px] box,
    // find nearest obstacle, apply exponential cost decay.
    static void cpu_inflate(const std::vector<uint8_t>& input,
                            std::vector<uint8_t>& output,
                            int W, int H, int radius_px,
                            float decay, float resolution)
    {
        for (int gy = 0; gy < H; ++gy) {
            for (int gx = 0; gx < W; ++gx) {
                size_t idx = static_cast<size_t>(gy) * W + gx;

                if (input[idx] != 0) {
                    output[idx] = 255;
                    continue;
                }

                float min_dist_sq = static_cast<float>(radius_px + 1) *
                                    static_cast<float>(radius_px + 1);

                int x0 = std::max(0, gx - radius_px);
                int x1 = std::min(W - 1, gx + radius_px);
                int y0 = std::max(0, gy - radius_px);
                int y1 = std::min(H - 1, gy + radius_px);

                for (int ny = y0; ny <= y1; ++ny) {
                    for (int nx = x0; nx <= x1; ++nx) {
                        if (input[static_cast<size_t>(ny) * W + nx] != 0) {
                            float dx   = static_cast<float>(gx - nx);
                            float dy   = static_cast<float>(gy - ny);
                            float dsq  = dx * dx + dy * dy;
                            if (dsq < min_dist_sq) min_dist_sq = dsq;
                        }
                    }
                }

                float radius_sq = static_cast<float>(radius_px) *
                                  static_cast<float>(radius_px);
                if (min_dist_sq > radius_sq) {
                    output[idx] = 0;
                } else {
                    float dist_m = std::sqrt(min_dist_sq) * resolution;
                    float cost   = 255.0f * std::exp(-decay * dist_m);
                    output[idx] = static_cast<uint8_t>(
                        std::clamp(static_cast<int>(cost), 1, 254));
                }
            }
        }
    }

    // ── run_gpu_kernel ────────────────────────────────────────────────────────
    // Allocate buffers, dispatch kernel, readback, return GPU time in ms.
    double run_gpu_kernel(cl::Kernel& kernel,
                          const std::vector<uint8_t>& input,
                          std::vector<uint8_t>& output,
                          int W, int H, int radius_px,
                          float decay, float resolution)
    {
        const size_t buf_bytes = static_cast<size_t>(W) * H;

        // WHY CL_MEM_COPY_HOST_PTR: copies obstacle_map into device memory at
        // buffer creation, avoiding a separate enqueueWriteBuffer call.
        cl::Buffer input_buf(ocl_.context,
                             CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                             buf_bytes,
                             const_cast<uint8_t*>(input.data()));

        cl::Buffer output_buf(ocl_.context, CL_MEM_WRITE_ONLY, buf_bytes);

        CL_CHECK(kernel.setArg(0, input_buf));
        CL_CHECK(kernel.setArg(1, output_buf));
        CL_CHECK(kernel.setArg(2, cl_int(W)));
        CL_CHECK(kernel.setArg(3, cl_int(H)));
        CL_CHECK(kernel.setArg(4, cl_int(radius_px)));
        CL_CHECK(kernel.setArg(5, cl_float(decay)));
        CL_CHECK(kernel.setArg(6, cl_float(resolution)));

        cl::Event ev;
        CL_CHECK(queue_.enqueueNDRangeKernel(
            kernel,
            cl::NullRange,
            cl::NDRange(static_cast<size_t>(W), static_cast<size_t>(H)),
            cl::NullRange,
            nullptr,
            &ev));
        // WHY finish() before readback: drains the queue so profiling timestamps
        // are committed and output_buf is fully written before host reads it.
        CL_CHECK(queue_.finish());

        CL_CHECK(queue_.enqueueReadBuffer(output_buf, CL_TRUE, 0,
                                          buf_bytes, output.data()));

        return duration_ms(ev);
    }

    // ── run_gpu_kernel_tiled ──────────────────────────────────────────────────
    // Like run_gpu_kernel() but pads the global NDRange to a multiple of
    // TILE_W × TILE_H and passes the local size to the enqueue call.
    // Arg 7 (__local tile) must be set by the caller before this method is
    // called — we do not set it here to avoid overwriting the host-allocated
    // local memory size.
    double run_gpu_kernel_tiled(cl::Kernel& kernel,
                                const std::vector<uint8_t>& input,
                                std::vector<uint8_t>& output,
                                int W, int H, int radius_px,
                                float decay, float resolution,
                                int tile_w, int tile_h)
    {
        const size_t buf_bytes = static_cast<size_t>(W) * H;

        // WHY CL_MEM_COPY_HOST_PTR: same as run_gpu_kernel — avoids a separate
        // enqueueWriteBuffer call; data is already on the host at this point.
        cl::Buffer input_buf(ocl_.context,
                             CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR,
                             buf_bytes,
                             const_cast<uint8_t*>(input.data()));

        cl::Buffer output_buf(ocl_.context, CL_MEM_WRITE_ONLY, buf_bytes);

        CL_CHECK(kernel.setArg(0, input_buf));
        CL_CHECK(kernel.setArg(1, output_buf));
        CL_CHECK(kernel.setArg(2, cl_int(W)));
        CL_CHECK(kernel.setArg(3, cl_int(H)));
        CL_CHECK(kernel.setArg(4, cl_int(radius_px)));
        CL_CHECK(kernel.setArg(5, cl_float(decay)));
        CL_CHECK(kernel.setArg(6, cl_float(resolution)));
        // Arg 7 (__local tile) already set by caller.

        // WHY padded global size: the local size must evenly divide the global
        // size for a 2D NDRange.  Padding to the next tile multiple ensures every
        // work-group is full; out-of-bounds work-items exit early via the guard
        // inside the kernel.
        size_t gw = static_cast<size_t>((W + tile_w - 1) / tile_w) * tile_w;
        size_t gh = static_cast<size_t>((H + tile_h - 1) / tile_h) * tile_h;

        cl::Event ev;
        CL_CHECK(queue_.enqueueNDRangeKernel(
            kernel,
            cl::NullRange,
            cl::NDRange(gw, gh),
            cl::NDRange(static_cast<size_t>(tile_w), static_cast<size_t>(tile_h)),
            nullptr,
            &ev));
        // WHY finish() before readback: see run_gpu_kernel().
        CL_CHECK(queue_.finish());

        CL_CHECK(queue_.enqueueReadBuffer(output_buf, CL_TRUE, 0,
                                          buf_bytes, output.data()));

        return duration_ms(ev);
    }

    // ── save_colorized_bmp ────────────────────────────────────────────────────
    // RGBA: obstacles→black, inflated cost>0→red gradient, free→white.
    static void save_colorized_bmp(const std::vector<uint8_t>& obstacle_map,
                                   const std::vector<uint8_t>& cost_map,
                                   int W, int H)
    {
        const size_t n = static_cast<size_t>(W) * H;
        std::vector<uint8_t> rgba(n * 4);

        for (size_t i = 0; i < n; ++i) {
            uint8_t cost = cost_map[i];
            if (obstacle_map[i] != 0) {
                // Obstacle — black
                rgba[i*4+0] = 0;   rgba[i*4+1] = 0;
                rgba[i*4+2] = 0;   rgba[i*4+3] = 255;
            } else if (cost > 0) {
                // Inflated zone — red→white gradient: high cost=red, low cost=white
                uint8_t fade = static_cast<uint8_t>(255 - cost);
                rgba[i*4+0] = 255; rgba[i*4+1] = fade;
                rgba[i*4+2] = fade; rgba[i*4+3] = 255;
            } else {
                // Free space — white
                rgba[i*4+0] = 255; rgba[i*4+1] = 255;
                rgba[i*4+2] = 255; rgba[i*4+3] = 255;
            }
        }

        if (!stbi_write_bmp("output_costmap.bmp", W, H, 4, rgba.data())) {
            throw std::runtime_error("stbi_write_bmp failed: output_costmap.bmp");
        }
        RCLCPP_INFO(rclcpp::get_logger("costmap_node"),
                    "[BMP] Saved output_costmap.bmp (%dx%d).", W, H);
    }

    // ── Tile dimensions — must match kernel -D macros passed in on_configure ───
    // WHY class-level: shared between on_configure (build_program -D flags) and
    // on_map_received (local-mem guard and NDRange sizing).
    static constexpr int TILE_W = 16;
    static constexpr int TILE_H = 16;

    // ── OpenCL members (valid between on_configure and on_cleanup) ────────────
    OclContext       ocl_;
    cl::CommandQueue queue_;
    cl::Program      program_naive_;
    cl::Program      program_tiled_;
    cl::Kernel       inflate_kernel_;
    cl::Kernel       inflate_tiled_kernel_;

    // ── Parameters ────────────────────────────────────────────────────────────
    std::string map_path_;
    double      inflation_radius_ = 0.5;
    double      resolution_       = 0.05;
    double      decay_            = 3.0;

    // ── ROS 2 pub/sub (valid between on_activate and on_deactivate) ──────────
    rclcpp::Subscription<OccupancyGrid>::SharedPtr sub_;
    rclcpp_lifecycle::LifecyclePublisher<OccupancyGrid>::SharedPtr pub_;

    // One-shot timer for out-of-band state transition (avoids mutex deadlock).
    rclcpp::TimerBase::SharedPtr shutdown_timer_;

    // Param validation callback handle — must outlive the node.
    rclcpp::node_interfaces::OnSetParametersCallbackHandle::SharedPtr param_cb_handle_;
};

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);

    // WHY use_intra_process_comms(true) on both nodes: intra-process delivery
    // routes the OccupancyGrid as a shared_ptr with zero DDS serialization.
    // Both nodes must opt in — one opting out forces the full serialization path.
    rclcpp::NodeOptions node_opts;
    node_opts.use_intra_process_comms(true);

    auto executor = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();

    // Create CostmapNode first so we can read map_path after on_configure().
    auto costmap_node = std::make_shared<CostmapNode>(node_opts);
    executor->add_node(costmap_node->get_node_base_interface());

    // Drive to INACTIVE (on_configure runs synchronously inside trigger_transition).
    costmap_node->trigger_transition(
        lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE);
    executor->spin_some(std::chrono::milliseconds(10));

    if (costmap_node->get_current_state().id() !=
            lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE) {
        throw std::runtime_error(
            "CostmapNode did not reach INACTIVE after TRANSITION_CONFIGURE");
    }

    // Drive to ACTIVE (on_activate creates sub + pub).
    costmap_node->trigger_transition(
        lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE);
    executor->spin_some(std::chrono::milliseconds(10));

    if (costmap_node->get_current_state().id() !=
            lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
        throw std::runtime_error(
            "CostmapNode did not reach ACTIVE after TRANSITION_ACTIVATE");
    }

    // Now that CostmapNode's subscription is live, add MapPublisher.
    // It fires its one-shot timer 10 ms after construction, which gives the
    // executor time to process the subscription registration first.
    auto pub_node = std::make_shared<MapPublisher>(
        costmap_node->map_path(), node_opts);
    executor->add_node(pub_node);

    // Runs until rclcpp::shutdown() is called from CostmapNode's shutdown timer.
    executor->spin();

    return 0;
}
