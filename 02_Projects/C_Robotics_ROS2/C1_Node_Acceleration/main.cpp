// C1 Node Acceleration — Applied OpenCL Lab
//
// Demonstrates two explicit goals from the design doc:
//   1. LifecycleNode pattern: OpenCL resources are allocated in on_configure()
//      and released in on_cleanup() — the same pattern C3 inherits for real
//      sensor-driven subscribers.
//   2. Intra-process pub/sub: SyntheticPublisher and AccelNode share the same
//      SingleThreadedExecutor with use_intra_process_comms(true), so the
//      Float32MultiArray is delivered as a shared_ptr with zero serialization.

#include "ocl_wrapper.hpp"   // create_context(), OclContext
#include "opencl_utils.hpp"  // CL_CHECK, build_program, duration_ms, get_binary_dir

#include <lifecycle_msgs/msg/state.hpp>
#include <lifecycle_msgs/msg/transition.hpp>
#include <rclcpp/rclcpp.hpp>
#include <rclcpp_lifecycle/lifecycle_node.hpp>
#include <std_msgs/msg/float32_multi_array.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <vector>

namespace fs = std::filesystem;
using Clock  = std::chrono::steady_clock;
using CallbackReturn = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface::CallbackReturn;
using Float32MultiArray = std_msgs::msg::Float32MultiArray;

// ─────────────────────────────────────────────────────────────────────────────
// AccelNode
// ─────────────────────────────────────────────────────────────────────────────
// Subclasses LifecycleNode so the host can drive resource allocation and
// teardown through well-defined state transitions instead of hiding them in
// the constructor/destructor.
class AccelNode : public rclcpp_lifecycle::LifecycleNode {
public:
    // Exposed so main() can read them before constructing SyntheticPublisher.
    int iterations_  = 10;
    int buffer_size_ = 1'048'576;

    explicit AccelNode(const rclcpp::NodeOptions& opts)
        : rclcpp_lifecycle::LifecycleNode("accel_node", opts)
    {}

    // ── on_configure ─────────────────────────────────────────────────────────
    // One-time cost: OpenCL context + queue + kernel + pre-allocated buffers.
    // WHY here and not the constructor: LifecycleNode separates construction
    // from initialization so the host can control when heavy resources are
    // acquired (e.g., after param server sync in a real system).
    CallbackReturn on_configure(const rclcpp_lifecycle::State&) override
    {
        // Read node parameters (set via CLI: --ros-args -p iterations:=20).
        declare_parameter("iterations",  iterations_);
        declare_parameter("buffer_size", buffer_size_);
        iterations_ = get_parameter("iterations").as_int();

        // §7.1 guard: as_int() returns int64_t — check before narrowing to int
        // so the guard actually fires when a large value is supplied at runtime.
        const int64_t buf_size_val = get_parameter("buffer_size").as_int();
        if (buf_size_val > static_cast<int64_t>(std::numeric_limits<int>::max())) {
            throw std::runtime_error("buffer_size exceeds INT_MAX");
        }
        buffer_size_ = static_cast<int>(buf_size_val);

        auto t0 = Clock::now();
        try {
            ocl_ = create_context();
        } catch (const std::exception& e) {
            RCLCPP_ERROR(get_logger(), "OpenCL init failed: %s", e.what());
            return CallbackReturn::FAILURE;
        }

        // WHY CL_QUEUE_PROFILING_ENABLE: required to read
        // CL_PROFILING_COMMAND_START / _END from cl::Event. Without this flag
        // the timestamps are undefined even if the event is captured.
        cl_int err = CL_SUCCESS;
        queue_ = cl::CommandQueue(ocl_.context, ocl_.device,
                                  CL_QUEUE_PROFILING_ENABLE, &err);
        CL_CHECK(err);

        fs::path kernel_path = get_binary_dir() / "kernels" / "passthrough.cl";
        auto prog = build_program(ocl_.context, ocl_.device, kernel_path.string());
        kernel_   = cl::Kernel(prog, "passthrough");

        // Pre-allocate buffers once; they are reused for every subscription
        // callback to avoid per-message allocation overhead.
        // WHY CL_MEM_READ_WRITE for both: passthrough reads src, writes dst.
        const size_t buf_bytes = static_cast<size_t>(buffer_size_) * sizeof(float);
        src_buf_ = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE, buf_bytes);
        dst_buf_ = cl::Buffer(ocl_.context, CL_MEM_READ_WRITE, buf_bytes);

        auto t1  = Clock::now();
        init_ms_ = std::chrono::duration<double, std::milli>(t1 - t0).count();
        RCLCPP_INFO(get_logger(), "[INIT] Context init: %.3f ms", init_ms_);

        callback_count_ = 0;
        dispatch_times_.clear();
        dispatch_times_.reserve(static_cast<size_t>(iterations_));

        return CallbackReturn::SUCCESS;
    }

    // ── on_activate ──────────────────────────────────────────────────────────
    // Create pub/sub only after OpenCL is ready. The publisher must be
    // explicitly activate()'d — rclcpp_lifecycle::LifecyclePublisher is
    // inactive by default to prevent premature publishing.
    CallbackReturn on_activate(const rclcpp_lifecycle::State&) override
    {
        pub_ = create_publisher<Float32MultiArray>("/processed_floats", 10);
        pub_->on_activate();

        // WHY lambda with 'this' capture: the subscription holds a reference
        // to this node via the node base; lifetime is managed by the executor.
        sub_ = create_subscription<Float32MultiArray>(
            "/raw_floats", 10,
            [this](Float32MultiArray::ConstSharedPtr msg) {
                this->process_callback(msg);
            });

        return CallbackReturn::SUCCESS;
    }

    // ── on_deactivate ─────────────────────────────────────────────────────────
    // Tear down pub/sub and print the timing summary. Called either by the
    // self-transition timer after all callbacks complete, or externally.
    CallbackReturn on_deactivate(const rclcpp_lifecycle::State&) override
    {
        sub_.reset();
        pub_->on_deactivate();
        pub_.reset();

        print_summary();
        return CallbackReturn::SUCCESS;
    }

    // ── on_cleanup ────────────────────────────────────────────────────────────
    // Release OpenCL resources explicitly via default-constructed assignment.
    // WHY explicit reset order (kernel/queue before context): OpenCL objects
    // hold internal references to their parent context; releasing them first
    // avoids dangling-reference warnings from some OpenCL runtimes.
    CallbackReturn on_cleanup(const rclcpp_lifecycle::State&) override
    {
        kernel_  = cl::Kernel();
        src_buf_ = cl::Buffer();
        dst_buf_ = cl::Buffer();
        queue_   = cl::CommandQueue();
        ocl_     = OclContext{};

        RCLCPP_INFO(get_logger(), "[DONE] OpenCL resources released.");
        return CallbackReturn::SUCCESS;
    }

private:
    // ── process_callback ─────────────────────────────────────────────────────
    // Runs on each intra-process message delivery from SyntheticPublisher.
    void process_callback(Float32MultiArray::ConstSharedPtr msg)
    {
        const int n = buffer_size_;
        const size_t buf_bytes = static_cast<size_t>(n) * sizeof(float);

        // §7.1: forbid silent truncation — the publisher is expected to send
        // exactly buffer_size_ floats. A smaller payload indicates a bug in
        // the upstream node, not a recoverable condition.
        if (msg->data.size() < static_cast<size_t>(buffer_size_)) {
            throw std::runtime_error(
                "process_callback: message payload smaller than buffer_size_");
        }

        CL_CHECK(queue_.enqueueWriteBuffer(src_buf_, CL_FALSE, 0,
                                           buf_bytes, msg->data.data()));

        CL_CHECK(kernel_.setArg(0, src_buf_));
        CL_CHECK(kernel_.setArg(1, dst_buf_));
        CL_CHECK(kernel_.setArg(2, cl_int(n)));

        cl::Event ev;
        CL_CHECK(queue_.enqueueNDRangeKernel(
            kernel_,
            cl::NullRange,
            cl::NDRange(static_cast<size_t>(n)),
            cl::NullRange,   // driver picks local size
            nullptr,
            &ev));
        // WHY finish() before CL_TRUE read: finish() drains the queue and
        // commits the profiling timestamps (START/END) so duration_ms(ev) is
        // valid. CL_TRUE blocks until host_dst is fully populated, ensuring
        // the std::move into the published message sees complete data.
        CL_CHECK(queue_.finish());

        std::vector<float> host_dst(static_cast<size_t>(n));
        CL_CHECK(queue_.enqueueReadBuffer(dst_buf_, CL_TRUE, 0,
                                          buf_bytes, host_dst.data()));

        double ms = duration_ms(ev);
        dispatch_times_.push_back(ms);

        RCLCPP_INFO(get_logger(), "[CB %2d] Dispatch: %.3f ms",
                    callback_count_ + 1, ms);

        // Publish processed result for downstream consumers.
        auto out = std::make_unique<Float32MultiArray>();
        out->data = std::move(host_dst);
        pub_->publish(std::move(out));

        ++callback_count_;
        if (callback_count_ >= iterations_) {
            // WHY one-shot timer (not direct trigger_transition call): calling
            // trigger_transition from inside a lifecycle callback deadlocks
            // because the state machine mutex is already held. The zero-duration
            // timer fires on the next executor iteration after this callback
            // returns, releasing the lock first.
            shutdown_timer_ = create_wall_timer(
                std::chrono::milliseconds(0),
                [this]() {
                    shutdown_timer_->cancel();

                    trigger_transition(
                        lifecycle_msgs::msg::Transition::TRANSITION_DEACTIVATE);
                    // Verify the state machine reached INACTIVE; anything else
                    // means on_deactivate returned FAILURE or an error occurred.
                    if (get_current_state().id() !=
                            lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE) {
                        RCLCPP_ERROR(get_logger(),
                            "TRANSITION_DEACTIVATE did not reach INACTIVE "
                            "(current state id: %u)",
                            get_current_state().id());
                    }

                    trigger_transition(
                        lifecycle_msgs::msg::Transition::TRANSITION_CLEANUP);
                    // Verify the state machine reached UNCONFIGURED.
                    if (get_current_state().id() !=
                            lifecycle_msgs::msg::State::PRIMARY_STATE_UNCONFIGURED) {
                        RCLCPP_ERROR(get_logger(),
                            "TRANSITION_CLEANUP did not reach UNCONFIGURED "
                            "(current state id: %u)",
                            get_current_state().id());
                    }

                    rclcpp::shutdown();
                });
        }
    }

    void print_summary() const
    {
        if (dispatch_times_.empty()) return;

        double avg = std::accumulate(dispatch_times_.begin(),
                                     dispatch_times_.end(), 0.0)
                     / static_cast<double>(dispatch_times_.size());
        double mn  = *std::min_element(dispatch_times_.begin(), dispatch_times_.end());
        double mx  = *std::max_element(dispatch_times_.begin(), dispatch_times_.end());

        std::cout << std::fixed << std::setprecision(3)
            << "+----------------------------------+\n"
               "|  C1 Node Acceleration Summary    |\n"
               "+--------------+-------------------+\n"
            << "| Context init | " << std::setw(11) << init_ms_ << " ms    |\n"
            << "| CB avg       | " << std::setw(11) << avg      << " ms    |\n"
            << "| CB min       | " << std::setw(11) << mn       << " ms    |\n"
            << "| CB max       | " << std::setw(11) << mx       << " ms    |\n"
               "+--------------+-------------------+\n";
    }

    // ── OpenCL members (valid between on_configure and on_cleanup) ────────────
    OclContext        ocl_;
    cl::CommandQueue  queue_;
    cl::Kernel        kernel_;
    cl::Buffer        src_buf_;
    cl::Buffer        dst_buf_;

    // ── Timing / accounting ───────────────────────────────────────────────────
    double              init_ms_        = 0.0;
    int                 callback_count_ = 0;
    std::vector<double> dispatch_times_;

    // ── ROS 2 pub/sub (valid between on_activate and on_deactivate) ──────────
    rclcpp::Subscription<Float32MultiArray>::SharedPtr sub_;
    rclcpp_lifecycle::LifecyclePublisher<Float32MultiArray>::SharedPtr pub_;

    // One-shot timer used to post the self-deactivation out-of-band.
    rclcpp::TimerBase::SharedPtr shutdown_timer_;
};

// ─────────────────────────────────────────────────────────────────────────────
// SyntheticPublisher
// ─────────────────────────────────────────────────────────────────────────────
// Companion node that drives AccelNode by publishing Float32MultiArray messages
// to /raw_floats. Running in the same executor with intra-process comms enabled
// means the message is delivered as a shared_ptr — no DDS serialization.
class SyntheticPublisher : public rclcpp::Node {
public:
    SyntheticPublisher(int iterations, int buffer_size,
                       const rclcpp::NodeOptions& opts)
        : rclcpp::Node("synthetic_publisher", opts)
        , iterations_(iterations)
        , buffer_size_(buffer_size)
    {
        pub_ = create_publisher<Float32MultiArray>("/raw_floats", 10);

        // WHY 50 ms period: gives the AccelNode subscription callback time to
        // process each message before the next one arrives, avoiding queue
        // buildup while keeping the demo short.
        timer_ = create_wall_timer(
            std::chrono::milliseconds(50),
            [this]() { publish_once(); });
    }

private:
    void publish_once()
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

    int iterations_;
    int buffer_size_;
    int publish_count_ = 0;
    rclcpp::Publisher<Float32MultiArray>::SharedPtr pub_;
    rclcpp::TimerBase::SharedPtr timer_;
};

// ─────────────────────────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);

    // WHY use_intra_process_comms(true) on both nodes: intra-process delivery
    // routes the message as a shared_ptr directly to the subscriber callback,
    // bypassing DDS serialization. Both nodes must opt in for the optimization
    // to activate — one node opting out forces the full serialization path.
    rclcpp::NodeOptions node_opts;
    node_opts.use_intra_process_comms(true);

    auto executor = std::make_shared<rclcpp::executors::SingleThreadedExecutor>();

    auto accel_node = std::make_shared<AccelNode>(node_opts);
    executor->add_node(accel_node->get_node_base_interface());

    // Drive AccelNode through configure → activate BEFORE adding
    // SyntheticPublisher so no messages are published before the subscription
    // is created in on_activate().
    accel_node->trigger_transition(
        lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE);
    executor->spin_some(std::chrono::milliseconds(10));

    // Verify on_configure completed and the node reached INACTIVE state.
    // spin_some() gives the executor a window to process any pending callbacks,
    // but on_configure runs synchronously inside trigger_transition, so the
    // state is already final here; the check guards against FAILURE returns.
    if (accel_node->get_current_state().id() !=
            lifecycle_msgs::msg::State::PRIMARY_STATE_INACTIVE) {
        throw std::runtime_error(
            "AccelNode did not reach INACTIVE after TRANSITION_CONFIGURE");
    }

    accel_node->trigger_transition(
        lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE);
    executor->spin_some(std::chrono::milliseconds(10));

    // Verify on_activate completed and the node reached ACTIVE state.
    if (accel_node->get_current_state().id() !=
            lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
        throw std::runtime_error(
            "AccelNode did not reach ACTIVE after TRANSITION_ACTIVATE");
    }

    // Add the publisher only after AccelNode is active and its subscription
    // is registered — this prevents early publishes from being dropped.
    // WHY accel_node->iterations_: SyntheticPublisher needs the final (parameter-
    // resolved) iteration count, which is available after on_configure().
    auto synth_node = std::make_shared<SyntheticPublisher>(
        accel_node->iterations_,
        accel_node->buffer_size_,
        node_opts);
    executor->add_node(synth_node);

    // Runs until rclcpp::shutdown() is called from AccelNode's shutdown timer.
    executor->spin();

    return 0;
}
