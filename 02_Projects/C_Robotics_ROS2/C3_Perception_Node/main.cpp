// main.cpp — C3 Perception Node (Applied OpenCL Lab)
//
// Demonstrates:
//   1. LifecycleNode with OpenCL resources allocated in on_configure().
//   2. Four-stage GPU pipeline per PointCloud2 message:
//      upload → filter → compact (prefix-sum) → feature extract → download → publish.
//   3. Per-stage timing: GPU stages via cl::Event, CPU stages via steady_clock.
//   4. /filtered_points and /cluster_features published as sensor_msgs/PointCloud2.
//   5. (Debug) CPU compaction correctness check vs GPU prefix-sum result.
//   6. (Challenge) Non-blocking double-buffer path via use_double_buffer:=true.

#include "ocl_wrapper.hpp"   // create_context(), OclContext
#include "opencl_utils.hpp"  // CL_CHECK, build_program, duration_ms, get_binary_dir

#include <lifecycle_msgs/msg/state.hpp>
#include <lifecycle_msgs/msg/transition.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <sensor_msgs/msg/point_cloud2.hpp>
#include <sensor_msgs/msg/point_field.hpp>
#include <std_msgs/msg/header.hpp>

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Clock          = std::chrono::steady_clock;
using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;
using PointCloud2    = sensor_msgs::msg::PointCloud2;
using PointField     = sensor_msgs::msg::PointField;

// Number of float32 accumulators for feature extraction (x,y,z,intensity,count).
static constexpr int FEATURE_ACCUM_COUNT = 5;
// Fixed-point scale used in feature_extract.cl (must match kernel define).
static constexpr float FEATURE_SCALE = 1000.0f;
// Blelloch prefix-sum kernel local size (must match LOCAL_SIZE in prefix_sum.cl).
static constexpr int PREFIX_LOCAL_SIZE = 128;

// ─────────────────────────────────────────────────────────────────────────────
// PerceptionNode
// ─────────────────────────────────────────────────────────────────────────────
class PerceptionNode : public rclcpp_lifecycle::LifecycleNode {
public:
    explicit PerceptionNode(const rclcpp::NodeOptions& opts)
        : rclcpp_lifecycle::LifecycleNode("perception_node", opts)
    {}

    // ── on_configure ──────────────────────────────────────────────────────────
    // One-time cost: OpenCL context init, kernel compilation, buffer allocation.
    CallbackReturn on_configure(const rclcpp_lifecycle::State&) override
    {
        // Declare node parameters (introspectable via ros2 param list).
        declare_parameter("topic",             std::string("/points"));
        declare_parameter("ground_z",          0.2);
        declare_parameter("min_intensity",     10.0);
        declare_parameter("max_points",        int64_t(100000));
        declare_parameter("use_double_buffer", false);

        topic_             = get_parameter("topic").as_string();
        ground_z_          = static_cast<float>(get_parameter("ground_z").as_double());
        min_intensity_     = static_cast<float>(get_parameter("min_intensity").as_double());
        use_double_buffer_ = get_parameter("use_double_buffer").as_bool();

        const int64_t max_pts_val = get_parameter("max_points").as_int();
        if (max_pts_val > static_cast<int64_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("max_points exceeds INT_MAX");
        }
        max_points_ = static_cast<int>(max_pts_val);

        auto t0 = Clock::now();
        try {
            ocl_ = create_context();
        } catch (const std::exception& e) {
            RCLCPP_ERROR(get_logger(), "OpenCL init failed: %s", e.what());
            return CallbackReturn::FAILURE;
        }

        // WHY CL_QUEUE_PROFILING_ENABLE: timestamps are undefined without this
        // flag; cl::Event profiling calls would return garbage values.
        cl_int err = CL_SUCCESS;
        queue_ = cl::CommandQueue(ocl_.context, ocl_.device,
                                  CL_QUEUE_PROFILING_ENABLE, &err);
        CL_CHECK(err);

        fs::path kdir = get_binary_dir() / "kernels";

        prog_filter_  = build_program(ocl_.context, ocl_.device,
                                      (kdir / "filter.cl").string());
        prog_scan_    = build_program(ocl_.context, ocl_.device,
                                      (kdir / "prefix_sum.cl").string());
        prog_feature_ = build_program(ocl_.context, ocl_.device,
                                      (kdir / "feature_extract.cl").string());

        filter_kernel_      = cl::Kernel(prog_filter_,  "filter_points");
        scan_tile_kernel_   = cl::Kernel(prog_scan_,    "prefix_sum_tile");
        scan_add_kernel_    = cl::Kernel(prog_scan_,    "add_tile_offsets");
        scatter_kernel_     = cl::Kernel(prog_scan_,    "scatter_compact");
        feature_kernel_     = cl::Kernel(prog_feature_, "feature_extract");

        // Pre-allocate device buffers sized for max_points_.
        // Default test layout: XYZ + intensity = 4 floats = 16 bytes/point.
        // We allocate for the maximum; actual usage is determined per message.
        // WHY static_cast<size_t>(max_points_) before multiply (§7.1):
        //   avoids signed int overflow when max_points_ * 4 * sizeof(float)
        //   exceeds INT_MAX.
        const size_t max_floats = static_cast<size_t>(max_points_) * 4;  // XYZ+I
        const size_t max_bytes  = max_floats * sizeof(float);

        const size_t max_tiles = (static_cast<size_t>(max_points_) +
                                  static_cast<size_t>(2 * PREFIX_LOCAL_SIZE) - 1)
                                 / static_cast<size_t>(2 * PREFIX_LOCAL_SIZE);

        // Slot [0] is always allocated (single-buffer and double-buffer paths).
        point_buf_[0]     = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE, max_bytes);
        compact_buf_[0]   = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE, max_bytes);
        mask_buf_[0]      = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE,
                                       static_cast<size_t>(max_points_) * sizeof(cl_uchar));
        // Scan operates on int array (copy of mask). Pre-allocate max size.
        scan_buf_[0]      = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE,
                                       static_cast<size_t>(max_points_) * sizeof(cl_int));
        // Tile sums: one int per tile (ceil(max_points / TILE_SIZE)).
        // WHY +1 in max_tiles expression: integer division truncates; +1 ensures
        //   enough slots even when max_points_ is not a multiple of TILE_SIZE.
        tile_sums_buf_[0] = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE,
                                       max_tiles * sizeof(cl_int));
        // Feature accumulators: 5 ints (x, y, z, intensity_sum, count).
        accum_buf_[0]     = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE,
                                       static_cast<size_t>(FEATURE_ACCUM_COUNT) * sizeof(cl_int));

        // Slot [1] is only allocated when the double-buffer path is enabled.
        // WHY separate allocation: the double-buffer design requires two
        // independent device memory regions so one frame can be uploaded/processed
        // while the previous frame's compact download is still in flight.
        if (use_double_buffer_) {
            point_buf_[1]     = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE, max_bytes);
            compact_buf_[1]   = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE, max_bytes);
            mask_buf_[1]      = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE,
                                           static_cast<size_t>(max_points_) * sizeof(cl_uchar));
            scan_buf_[1]      = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE,
                                           static_cast<size_t>(max_points_) * sizeof(cl_int));
            tile_sums_buf_[1] = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE,
                                           max_tiles * sizeof(cl_int));
            accum_buf_[1]     = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE,
                                           static_cast<size_t>(FEATURE_ACCUM_COUNT) * sizeof(cl_int));
        }

        auto t1     = Clock::now();
        double init_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        RCLCPP_INFO(get_logger(), "[INIT] Context + kernel compile: %.3f ms", init_ms);

        return CallbackReturn::SUCCESS;
    }

    // ── on_activate ───────────────────────────────────────────────────────────
    CallbackReturn on_activate(const rclcpp_lifecycle::State&) override
    {
        filtered_pub_ = create_publisher<PointCloud2>("/filtered_points", 10);
        features_pub_ = create_publisher<PointCloud2>("/cluster_features", 10);
        // WHY manual on_activate(): publishers created inside on_activate() are not
        // in the framework's pre-registered list, so they are never auto-activated
        // during the TRANSITION_ACTIVATE pass. Must be activated explicitly here.
        filtered_pub_->on_activate();
        features_pub_->on_activate();

        // Try loaned message subscription first; fall back to copy-based.
        // WHY warn-and-continue: loaned messages depend on RMW support.
        // Crashing here would break all non-fastrtps environments.
        try {
            sub_ = create_subscription<PointCloud2>(
                topic_, 10,
                [this](PointCloud2::UniquePtr msg) {
                    this->process_callback_loaned(std::move(msg));
                });
            RCLCPP_INFO(get_logger(), "[INIT] Subscribed to %s (loaned path)",
                        topic_.c_str());
        } catch (const std::exception& e) {
            RCLCPP_WARN(get_logger(),
                "Loaned message subscription unavailable (%s). "
                "Falling back to copy-based transport.", e.what());
            sub_ = create_subscription<PointCloud2>(
                topic_, 10,
                [this](PointCloud2::ConstSharedPtr msg) {
                    this->process_callback(msg);
                });
            RCLCPP_INFO(get_logger(), "[INIT] Subscribed to %s (copy-based path)",
                        topic_.c_str());
        }

        return CallbackReturn::SUCCESS;
    }

    // ── on_deactivate ─────────────────────────────────────────────────────────
    // WHY reset (not just on_deactivate): resetting to nullptr ensures a clean
    // re-activate cycle — re-creating publishers from scratch avoids stale
    // internal state from a previous activation.
    CallbackReturn on_deactivate(const rclcpp_lifecycle::State&) override
    {
        sub_.reset();
        filtered_pub_.reset();
        features_pub_.reset();
        return CallbackReturn::SUCCESS;
    }

    // ── on_cleanup ────────────────────────────────────────────────────────────
    // WHY explicit reset order (kernel → program → buffer → queue → context):
    // CL objects hold references to their parent context; releasing children
    // first avoids dangling-reference warnings from some OpenCL runtimes.
    CallbackReturn on_cleanup(const rclcpp_lifecycle::State&) override
    {
        filter_kernel_    = cl::Kernel();
        scan_tile_kernel_ = cl::Kernel();
        scan_add_kernel_  = cl::Kernel();
        scatter_kernel_   = cl::Kernel();
        feature_kernel_   = cl::Kernel();
        prog_filter_    = cl::Program();
        prog_scan_      = cl::Program();
        prog_feature_   = cl::Program();
        point_buf_[0]     = cl::Buffer();
        compact_buf_[0]   = cl::Buffer();
        mask_buf_[0]      = cl::Buffer();
        scan_buf_[0]      = cl::Buffer();
        tile_sums_buf_[0] = cl::Buffer();
        accum_buf_[0]     = cl::Buffer();
        if (use_double_buffer_) {
            point_buf_[1]     = cl::Buffer();
            compact_buf_[1]   = cl::Buffer();
            mask_buf_[1]      = cl::Buffer();
            scan_buf_[1]      = cl::Buffer();
            tile_sums_buf_[1] = cl::Buffer();
            accum_buf_[1]     = cl::Buffer();
        }
        queue_          = cl::CommandQueue();
        ocl_            = OclContext{};
        RCLCPP_INFO(get_logger(), "[DONE] OpenCL resources released.");
        return CallbackReturn::SUCCESS;
    }

private:
    // ── process_callback (copy-based path) ────────────────────────────────────
    void process_callback(PointCloud2::ConstSharedPtr msg)
    {
        dispatch(msg->data.data(), msg->width * msg->height,
                 msg->point_step, msg->header);
    }

    // ── process_callback_loaned (loaned/zero-copy path) ───────────────────────
    void process_callback_loaned(PointCloud2::UniquePtr msg)
    {
        dispatch(msg->data.data(), msg->width * msg->height,
                 msg->point_step, msg->header);
    }

    // ── dispatch ──────────────────────────────────────────────────────────────
    // Routes each incoming message to the blocking single-buffer path or the
    // non-blocking double-buffer path depending on use_double_buffer_.
    void dispatch(const uint8_t* data, uint32_t num_pts, uint32_t point_step,
                  const std_msgs::msg::Header& hdr)
    {
        if (!use_double_buffer_) {
            // Blocking single-buffer path: process buf[0] and wait for completion
            // before returning so that the next callback sees a consistent state.
            run_pipeline(data, num_pts, point_step, hdr, 0);
            CL_CHECK(queue_.finish());
            return;
        }

        // Non-blocking double-buffer path.
        //
        // Contention guard: if the previous async dispatch has not yet completed
        // (its compact-download event is still queued), fall back to a blocking
        // wait on the full queue rather than overwriting in-flight device buffers.
        // WHY check prev_done_ev_() != nullptr before getInfo:
        //   cl::Event is default-constructed with a null internal handle; calling
        //   getInfo on a null event is undefined behaviour in some runtimes.
        // Load write_idx before checking contention so the contention guard
        // sees the buffer index we intend to write, matching the spec ordering.
        int write_idx = active_buf_.load();

        cl_int ev_status = CL_COMPLETE;
        if (prev_done_ev_() != nullptr) {
            CL_CHECK(prev_done_ev_.getInfo(CL_EVENT_COMMAND_EXECUTION_STATUS, &ev_status));
        }
        if (ev_status != CL_COMPLETE) {
            CL_CHECK(queue_.finish());
            RCLCPP_WARN(get_logger(), "double-buffer contention — falling back to blocking wait");
            // After forced flush the in-flight callback fires, flipping active_buf_.
            // Reload to get the now-available write slot.
            write_idx = active_buf_.load();
        }
        prev_done_ev_ = run_pipeline(data, num_pts, point_step, hdr, write_idx);

        // WHY lambda captures only &active_buf_ via void* user:
        // cl::Event callback fires in the OpenCL driver thread —
        // only std::atomic operations are safe here; no RCLCPP calls, no mutex.
        // WHY CL_CHECK on setCallback: a failed registration silently breaks
        // the buffer-swap invariant — the atomic flip would never fire.
        CL_CHECK(prev_done_ev_.setCallback(CL_COMPLETE, [](cl_event, cl_int, void* user) {
            auto* ab = static_cast<std::atomic<int>*>(user);
            ab->store(1 - ab->load());
        }, &active_buf_));
    }

    // ── run_pipeline ──────────────────────────────────────────────────────────
    // Four-stage GPU pipeline with per-stage cl::Event profiling.
    // Returns the compact-download cl::Event so the caller can choose whether
    // to wait (blocking path) or register a completion callback (double-buffer).
    cl::Event run_pipeline(const uint8_t* data_ptr,
                           uint32_t num_points_u32,
                           uint32_t point_step,
                           const std_msgs::msg::Header& header,
                           int buf_idx)
    {
        // ── 0. Validate and derive sizes ──────────────────────────────────────
        if (num_points_u32 > static_cast<uint32_t>(max_points_)) {
            RCLCPP_WARN(get_logger(),
                "Message has %u points; pre-allocated buffer holds %d. Skipping.",
                num_points_u32, max_points_);
            return cl::Event{};
        }
        if (point_step % sizeof(float) != 0) {
            throw std::runtime_error("point_step is not a multiple of sizeof(float)");
        }
        if (point_step / sizeof(float) < 4) {
            throw std::runtime_error("point_step too small: need at least 4 floats (XYZI)");
        }

        const int    num_points        = static_cast<int>(num_points_u32);
        const int    point_step_floats = static_cast<int>(point_step / sizeof(float));
        // §7.1: promote point_step (size_t) before multiplying by num_points
        //   to avoid 32-bit signed overflow when point_step * num_points > INT_MAX.
        const size_t total_bytes       = static_cast<size_t>(point_step) * num_points;

        auto t_start = Clock::now();

        // ── 1. GPU Upload ─────────────────────────────────────────────────────
        cl::Event upload_ev;
        CL_CHECK(queue_.enqueueWriteBuffer(
            point_buf_[buf_idx], CL_FALSE, 0, total_bytes, data_ptr,
            nullptr, &upload_ev));

        // ── 2. GPU Filter ─────────────────────────────────────────────────────
        CL_CHECK(filter_kernel_.setArg(0, point_buf_[buf_idx]));
        CL_CHECK(filter_kernel_.setArg(1, mask_buf_[buf_idx]));
        CL_CHECK(filter_kernel_.setArg(2, cl_float(ground_z_)));
        CL_CHECK(filter_kernel_.setArg(3, cl_float(min_intensity_)));
        CL_CHECK(filter_kernel_.setArg(4, cl_int(point_step_floats)));
        CL_CHECK(filter_kernel_.setArg(5, cl_int(num_points)));

        cl::Event filter_ev;
        std::vector<cl::Event> upload_deps = {upload_ev};
        CL_CHECK(queue_.enqueueNDRangeKernel(
            filter_kernel_,
            cl::NullRange,
            cl::NDRange(static_cast<size_t>(num_points)),
            cl::NullRange,
            &upload_deps,
            &filter_ev));

        // ── 3. GPU Compact (prefix sum + scatter) ─────────────────────────────
        // Step 3a: copy uchar mask to int scan buffer (prefix_sum_tile
        //          operates on int[]).  We do this with a small host-side
        //          read-back + write to keep CL 1.2 compatibility (no
        //          device-to-device copy without an explicit kernel).
        // WHY finish() here: we need the filter mask on host to build the int
        //   copy for the scan. Blocking ensures mask_buf_ is fully written.
        CL_CHECK(queue_.finish());

        std::vector<cl_uchar> host_mask(static_cast<size_t>(num_points));
        CL_CHECK(queue_.enqueueReadBuffer(mask_buf_[buf_idx], CL_TRUE, 0,
                                          static_cast<size_t>(num_points) * sizeof(cl_uchar),
                                          host_mask.data()));

        std::vector<cl_int> host_scan(static_cast<size_t>(num_points));
        for (int i = 0; i < num_points; ++i) {
            host_scan[i] = static_cast<cl_int>(host_mask[i]);
        }

        // Upload int mask as scan_buf_.
        CL_CHECK(queue_.enqueueWriteBuffer(scan_buf_[buf_idx], CL_TRUE, 0,
                                           static_cast<size_t>(num_points) * sizeof(cl_int),
                                           host_scan.data()));

        // Step 3b: Tile-based Blelloch prefix sum on scan_buf_.
        //
        // Pass 1 — prefix_sum_tile: each work-group scans one tile of
        //   TILE_SIZE = 2 * LOCAL_SIZE elements independently and writes its
        //   total into tile_sums[].
        //
        // The tile_sums[] array is then exclusive-prefix-summed on the host
        // (typically O(10s) of tiles → negligible cost).
        //
        // Pass 2 — add_tile_offsets: each work-group adds tile_sums[group_id]
        //   to every element in its tile, completing the global scan.
        //
        // WHY tile-based (not single-group): supports arbitrary num_points
        //   without a CPU fallback path.

        const int TILE_SIZE  = 2 * PREFIX_LOCAL_SIZE;
        const int num_tiles  = (num_points + TILE_SIZE - 1) / TILE_SIZE;
        const size_t tile_local_bytes = static_cast<size_t>(TILE_SIZE) * sizeof(cl_int);

        // Pass 1.
        CL_CHECK(scan_tile_kernel_.setArg(0, scan_buf_[buf_idx]));
        CL_CHECK(scan_tile_kernel_.setArg(1, tile_sums_buf_[buf_idx]));
        CL_CHECK(scan_tile_kernel_.setArg(2, cl::Local(tile_local_bytes)));
        CL_CHECK(scan_tile_kernel_.setArg(3, cl_int(num_points)));

        cl::Event compact_ev;
        CL_CHECK(queue_.enqueueNDRangeKernel(
            scan_tile_kernel_,
            cl::NullRange,
            cl::NDRange(static_cast<size_t>(num_tiles) *
                        static_cast<size_t>(PREFIX_LOCAL_SIZE)),
            cl::NDRange(static_cast<size_t>(PREFIX_LOCAL_SIZE)),
            nullptr,
            &compact_ev));
        CL_CHECK(queue_.finish());

        // Read per-tile totals, compute their exclusive prefix sum on host.
        std::vector<cl_int> tile_sums_host(static_cast<size_t>(num_tiles));
        CL_CHECK(queue_.enqueueReadBuffer(tile_sums_buf_[buf_idx], CL_TRUE, 0,
                                          static_cast<size_t>(num_tiles) * sizeof(cl_int),
                                          tile_sums_host.data()));

        cl_int running_tile = 0;
        for (int t = 0; t < num_tiles; ++t) {
            cl_int orig       = tile_sums_host[t];
            tile_sums_host[t] = running_tile;
            running_tile     += orig;
        }
        const int compact_count = running_tile;

        CL_CHECK(queue_.enqueueWriteBuffer(tile_sums_buf_[buf_idx], CL_TRUE, 0,
                                           static_cast<size_t>(num_tiles) * sizeof(cl_int),
                                           tile_sums_host.data()));

        // Pass 2: add tile offsets to complete the global scan.
        // WHY skip when num_tiles == 1: all data is already in the one tile's
        //   local exclusive scan — no inter-tile offset to add.
        cl::Event scan_add_ev;
        if (num_tiles > 1) {
            CL_CHECK(scan_add_kernel_.setArg(0, scan_buf_[buf_idx]));
            CL_CHECK(scan_add_kernel_.setArg(1, tile_sums_buf_[buf_idx]));
            CL_CHECK(scan_add_kernel_.setArg(2, cl_int(num_points)));

            CL_CHECK(queue_.enqueueNDRangeKernel(
                scan_add_kernel_,
                cl::NullRange,
                cl::NDRange(static_cast<size_t>(num_tiles) *
                            static_cast<size_t>(PREFIX_LOCAL_SIZE)),
                cl::NDRange(static_cast<size_t>(PREFIX_LOCAL_SIZE)),
                nullptr,
                &scan_add_ev));
            CL_CHECK(queue_.finish());
        }

        // Step 3c: Scatter compacted points.
        CL_CHECK(scatter_kernel_.setArg(0, point_buf_[buf_idx]));
        CL_CHECK(scatter_kernel_.setArg(1, scan_buf_[buf_idx]));
        CL_CHECK(scatter_kernel_.setArg(2, mask_buf_[buf_idx]));
        CL_CHECK(scatter_kernel_.setArg(3, compact_buf_[buf_idx]));
        CL_CHECK(scatter_kernel_.setArg(4, cl_int(point_step_floats)));
        CL_CHECK(scatter_kernel_.setArg(5, cl_int(num_points)));

        cl::Event scatter_ev;
        CL_CHECK(queue_.enqueueNDRangeKernel(
            scatter_kernel_,
            cl::NullRange,
            cl::NDRange(static_cast<size_t>(num_points)),
            cl::NullRange,
            nullptr,
            &scatter_ev));

        // ── 4. GPU Feature Extract ─────────────────────────────────────────────
        // Zero accumulators before dispatch (CL 1.2 compatible: enqueueWriteBuffer).
        std::vector<cl_int> zero_accum(FEATURE_ACCUM_COUNT, 0);
        CL_CHECK(queue_.enqueueWriteBuffer(accum_buf_[buf_idx], CL_TRUE, 0,
                                           static_cast<size_t>(FEATURE_ACCUM_COUNT) * sizeof(cl_int),
                                           zero_accum.data()));

        cl::Event feature_ev;
        if (compact_count > 0) {
            CL_CHECK(feature_kernel_.setArg(0, compact_buf_[buf_idx]));
            CL_CHECK(feature_kernel_.setArg(1, accum_buf_[buf_idx]));
            CL_CHECK(feature_kernel_.setArg(2, cl_int(point_step_floats)));
            CL_CHECK(feature_kernel_.setArg(3, cl_int(compact_count)));

            CL_CHECK(queue_.enqueueNDRangeKernel(
                feature_kernel_,
                cl::NullRange,
                cl::NDRange(static_cast<size_t>(compact_count)),
                cl::NullRange,
                nullptr,
                &feature_ev));
        }

        // ── 5. GPU Download ───────────────────────────────────────────────────
        // WHY CL_TRUE for accum: publish step reads accum_host immediately after
        //   this call — must be complete before we proceed to CPU publish.
        // WHY CL_FALSE for compact: the download event is returned to the caller
        //   so the double-buffer path can overlap it with the next frame's upload.
        const size_t compact_bytes = static_cast<size_t>(point_step) * compact_count;
        std::vector<uint8_t> compact_host(compact_bytes);
        std::vector<cl_int>  accum_host(FEATURE_ACCUM_COUNT, 0);

        cl::Event download_ev;
        if (compact_count > 0) {
            CL_CHECK(queue_.finish());
            CL_CHECK(queue_.enqueueReadBuffer(compact_buf_[buf_idx], CL_FALSE, 0,
                                              compact_bytes, compact_host.data(),
                                              nullptr, &download_ev));
        }
        CL_CHECK(queue_.enqueueReadBuffer(accum_buf_[buf_idx], CL_TRUE, 0,
                                          static_cast<size_t>(FEATURE_ACCUM_COUNT) * sizeof(cl_int),
                                          accum_host.data()));

        // ── 6. CPU Publish ────────────────────────────────────────────────────
        auto t_publish_start = Clock::now();

        // Publish /filtered_points
        {
            auto out      = std::make_unique<PointCloud2>();
            out->header   = header;
            out->height   = 1;
            out->width    = static_cast<uint32_t>(compact_count);
            out->point_step = point_step;
            out->row_step   = point_step * static_cast<uint32_t>(compact_count);
            out->is_dense   = true;
            // Preserve original field descriptors (from the original message header
            // we do not have here — use standard XYZI fields for the default layout).
            PointField fx, fy, fz, fi;
            fx.name = "x"; fx.offset = 0;            fx.datatype = PointField::FLOAT32; fx.count = 1;
            fy.name = "y"; fy.offset = 4;            fy.datatype = PointField::FLOAT32; fy.count = 1;
            fz.name = "z"; fz.offset = 8;            fz.datatype = PointField::FLOAT32; fz.count = 1;
            fi.name = "intensity"; fi.offset = 12;   fi.datatype = PointField::FLOAT32; fi.count = 1;
            out->fields = {fx, fy, fz, fi};
            out->data   = std::move(compact_host);
            filtered_pub_->publish(std::move(out));
        }

        // Publish /cluster_features only when there are filtered points.
        // WHY guard: publishing a zero-centroid feature on empty frames is misleading;
        // downstream consumers should interpret absence of message as "no clusters".
        // Fields: centroid XYZ + intensity_mean + count (all float32, 20 bytes/feature).
        if (compact_count > 0) {
            float cx = static_cast<float>(accum_host[0]) / FEATURE_SCALE / compact_count;
            float cy = static_cast<float>(accum_host[1]) / FEATURE_SCALE / compact_count;
            float cz = static_cast<float>(accum_host[2]) / FEATURE_SCALE / compact_count;
            float ci = static_cast<float>(accum_host[3]) / FEATURE_SCALE / compact_count;
            float count_f = static_cast<float>(compact_count);

            std::vector<uint8_t> feat_data(5 * sizeof(float));
            std::memcpy(feat_data.data() + 0  * sizeof(float), &cx,      sizeof(float));
            std::memcpy(feat_data.data() + 1  * sizeof(float), &cy,      sizeof(float));
            std::memcpy(feat_data.data() + 2  * sizeof(float), &cz,      sizeof(float));
            std::memcpy(feat_data.data() + 3  * sizeof(float), &ci,      sizeof(float));
            std::memcpy(feat_data.data() + 4  * sizeof(float), &count_f, sizeof(float));

            auto feat         = std::make_unique<PointCloud2>();
            feat->header      = header;
            feat->height      = 1;
            feat->width       = 1;   // one "feature point" per cluster
            feat->point_step  = 5 * sizeof(float);
            feat->row_step    = feat->point_step;
            feat->is_dense    = true;

            PointField fcx, fcy, fcz, fci, fcc;
            fcx.name = "x";             fcx.offset = 0;  fcx.datatype = PointField::FLOAT32; fcx.count = 1;
            fcy.name = "y";             fcy.offset = 4;  fcy.datatype = PointField::FLOAT32; fcy.count = 1;
            fcz.name = "z";             fcz.offset = 8;  fcz.datatype = PointField::FLOAT32; fcz.count = 1;
            fci.name = "intensity_mean";fci.offset = 12; fci.datatype = PointField::FLOAT32; fci.count = 1;
            fcc.name = "count";         fcc.offset = 16; fcc.datatype = PointField::FLOAT32; fcc.count = 1;
            feat->fields = {fcx, fcy, fcz, fci, fcc};
            feat->data   = std::move(feat_data);
            features_pub_->publish(std::move(feat));
        }

        auto t_end      = Clock::now();
        double publish_ms = std::chrono::duration<double, std::milli>(
                                t_end - t_publish_start).count();

        // ── 7. Timing table ───────────────────────────────────────────────────
        // Columns: upload | filter | compact | feature_extract | download | publish | total
        // WHY compact includes scatter: the scatter kernel is an integral part of
        //   the compaction stage; folding avoids a misleading extra column.
        double upload_ms  = duration_ms(upload_ev);
        double filter_ms  = duration_ms(filter_ev);
        // compact_ev captures Pass 1 of the tile scan (the dominant GPU work).
        // scan_add_ev captures Pass 2 (skipped when num_tiles == 1 → stays 0.0).
        // Scatter is appended to the same stage label.
        double compact_ms       = duration_ms(compact_ev);
        double scan_add_ms      = (num_tiles > 1) ? duration_ms(scan_add_ev) : 0.0;
        double scatter_ms       = duration_ms(scatter_ev);
        double compact_total_ms = compact_ms + scan_add_ms + scatter_ms;
        double feature_ms  = 0.0;
        double download_ms = 0.0;
        if (compact_count > 0) {
            feature_ms  = duration_ms(feature_ev);
            download_ms = duration_ms(download_ev);
        }
        double total_ms = std::chrono::duration<double, std::milli>(
                              t_end - t_start).count();

        msg_count_++;
        RCLCPP_INFO(get_logger(),
            "[MSG %4d] pts_in=%d pts_out=%d buf=%d | "
            "upload=%.3f filter=%.3f compact=%.3f "
            "feature=%.3f download=%.3f publish=%.3f | total=%.3f ms",
            msg_count_,
            num_points, compact_count, buf_idx,
            upload_ms, filter_ms, compact_total_ms,
            feature_ms, download_ms, publish_ms,
            total_ms);

        // ── 8. Debug: compaction correctness check ────────────────────────────
#ifndef NDEBUG
        if (msg_count_ == 1) {
            // CPU exclusive prefix sum of the same mask.
            std::vector<cl_int> cpu_scan(static_cast<size_t>(num_points));
            cl_int running = 0;
            for (int i = 0; i < num_points; ++i) {
                cpu_scan[i] = running;
                running    += static_cast<cl_int>(host_mask[i]);
            }
            int cpu_compact = running;

            if (cpu_compact == compact_count) {
                RCLCPP_INFO(get_logger(), "[DEBUG] COMPACTION OK (count=%d)", compact_count);
            } else {
                RCLCPP_WARN(get_logger(),
                    "[DEBUG] COMPACTION MISMATCH: GPU=%d CPU=%d",
                    compact_count, cpu_compact);
            }
        }
#endif

        return download_ev;
    }

    // ── OpenCL members (valid between on_configure and on_cleanup) ────────────
    OclContext       ocl_;
    cl::CommandQueue queue_;
    cl::Program      prog_filter_;
    cl::Program      prog_scan_;
    cl::Program      prog_feature_;
    cl::Kernel       filter_kernel_;
    cl::Kernel       scan_tile_kernel_;
    cl::Kernel       scan_add_kernel_;
    cl::Kernel       scatter_kernel_;
    cl::Kernel       feature_kernel_;
    // Double-buffered device memory: [0] always active; [1] allocated only when
    // use_double_buffer_ is true. active_buf_ selects the write slot.
    cl::Buffer       point_buf_[2];
    cl::Buffer       compact_buf_[2];
    cl::Buffer       mask_buf_[2];
    cl::Buffer       scan_buf_[2];
    cl::Buffer       tile_sums_buf_[2];
    cl::Buffer       accum_buf_[2];

    // ── Parameters ────────────────────────────────────────────────────────────
    std::string topic_             = "/points";
    float       ground_z_          = 0.2f;
    float       min_intensity_     = 10.0f;
    int         max_points_        = 100000;
    bool        use_double_buffer_ = false;

    // ── Double-buffer state ───────────────────────────────────────────────────
    // active_buf_ is flipped by the CL event callback in the driver thread.
    // WHY std::atomic: the callback fires outside the ROS executor thread;
    //   atomic load/store avoids a data race without a mutex.
    std::atomic<int> active_buf_{0};
    cl::Event        prev_done_ev_;

    // ── ROS 2 pub/sub (valid between on_activate and on_deactivate) ──────────
    rclcpp::Subscription<PointCloud2>::SharedPtr sub_;
    rclcpp_lifecycle::LifecyclePublisher<PointCloud2>::SharedPtr filtered_pub_;
    rclcpp_lifecycle::LifecyclePublisher<PointCloud2>::SharedPtr features_pub_;

    int msg_count_ = 0;
};

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    // Print parameter usage and exit if --help is requested.
    // (perception_node uses declare_parameter, not CLI11, so we handle --help manually.)
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--help" || std::string(argv[i]) == "-h") {
            std::cout <<
                "Usage: perception_node [ROS args]\n\n"
                "ROS 2 LifecycleNode — accelerated PointCloud2 processing via OpenCL.\n\n"
                "Node parameters (set via --ros-args -p <name>:=<value>):\n"
                "  topic              string  default: /points       — subscription topic\n"
                "  ground_z           double  default: 0.2           — ground removal threshold (m)\n"
                "  min_intensity      double  default: 10.0          — minimum intensity threshold\n"
                "  max_points         int64   default: 100000        — pre-allocated buffer size\n"
                "  use_double_buffer  bool    default: false         — enable non-blocking double-buffer path (C3 Challenge)\n\n"
                "Publishes:\n"
                "  /filtered_points   sensor_msgs/PointCloud2  — ground- and intensity-filtered cloud\n"
                "  /cluster_features  sensor_msgs/PointCloud2  — centroid XYZ, intensity mean, count\n\n"
                "GPU selection: set GPU=<vendor> env var (e.g. GPU=AMD, GPU=NVIDIA, GPU=INTEL).\n";
            return 0;
        }
    }

    rclcpp::init(argc, argv);

    // WHY default NodeOptions (not intra-process): perception_node is designed
    // to receive PointCloud2 from an external process (point_cloud_publisher or
    // a real sensor driver). Intra-process comms only help when both nodes run
    // in the same executor. Here we use DDS for realistic latency measurement.
    rclcpp::NodeOptions node_opts;

    auto executor = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();
    auto node     = std::make_shared<PerceptionNode>(node_opts);
    executor->add_node(node->get_node_base_interface());

    // Drive lifecycle: configure → activate.
    node->trigger_transition(
        lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE);
    executor->spin_some(std::chrono::milliseconds(10));

    if (node->get_current_state().id() !=
            lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE) {
        throw std::runtime_error(
            "PerceptionNode did not reach INACTIVE after TRANSITION_CONFIGURE");
    }

    node->trigger_transition(
        lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE);
    executor->spin_some(std::chrono::milliseconds(10));

    if (node->get_current_state().id() !=
            lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
        throw std::runtime_error(
            "PerceptionNode did not reach ACTIVE after TRANSITION_ACTIVATE");
    }

    RCLCPP_INFO(node->get_logger(),
        "PerceptionNode ACTIVE — waiting for PointCloud2 on %s",
        node->get_parameter("topic").as_string().c_str());

    // Runs until Ctrl-C.
    executor->spin();

    rclcpp::shutdown();
    return 0;
}
