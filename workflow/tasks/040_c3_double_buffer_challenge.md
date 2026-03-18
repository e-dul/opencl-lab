# Task 040: C3 Challenge — Double-Buffer Real-Time Guarantee

## Context
- **Design Feature:** `workflow/design/06-robotics-ros2-projects.md`
- **Milestone:** Phase 5 — C3 Challenge: Double-Buffer Real-Time Guarantee
- **Relevant Files:**
  - `02_Projects/C_Robotics_ROS2/C3_Perception_Node/main.cpp` — (to modify)
  - `workflow/design/06-robotics-ros2-projects.md` — (read-only: spec reference)

## Objective

Extend the existing `PerceptionNode` with a non-blocking double-buffer path, activated by `use_double_buffer:=true`, so that GPU processing of message N and ROS 2 callback handling of message N+1 can overlap — with a contention guard that falls back to `queue_.finish()` and logs a `WARN` when the GPU has not finished before the next message arrives.

## Constraints & Rules

- **No new binary, no new directory.** Same `perception_node` binary in `C3_Perception_Node/`. Do not add a new CMake target.
- **No new kernel files.** All kernels are unchanged; only host C++ in `main.cpp` changes.
- **`use_double_buffer` parameter:** declared in `on_configure()` via `declare_parameter("use_double_buffer", false)`. When `false`, the node behaves exactly as before (blocking `queue_.finish()` path, single buffer set). No behavioural change to the `false` path.
- **Atomic operations only in the `cl::Event` callback.** The callback executes in the OpenCL driver thread — only `std::atomic<>` operations are safe there. No mutexes, no RCLCPP calls.
- **Silent drop is forbidden.** If the active buffer cannot be determined (contention), call `queue_.finish()` and log `WARN: double-buffer contention — falling back to blocking wait`.
- **`CL_CHECK` coverage:** all `setArg`, `enqueueNDRangeKernel`, `enqueueReadBuffer`, `enqueueWriteBuffer`, `finish` calls must be wrapped (master spec §7.2).
- **Integer overflow safety:** use `static_cast<size_t>(max_points_)` before multiplying (master spec §7.1).

---

## Implementation

1. **Add parameter declaration** in `on_configure()`:
   ```cpp
   declare_parameter("use_double_buffer", false);
   use_double_buffer_ = get_parameter("use_double_buffer").as_bool();
   ```

2. **Add second buffer pair** in `on_configure()`, gated on `use_double_buffer_`:
   - Allocate index `[1]` for `point_buf_[2]`, `compact_buf_[2]`, `mask_buf_[2]`, `scan_buf_[2]`, `tile_sums_buf_[2]`, `accum_buf_[2]`.
   - Index `[0]` is always allocated (for both single and double-buffer modes).
   - When `use_double_buffer_` is `false`, only `[0]` is allocated; `[1]` stays default-constructed.

3. **Change buffer member declarations** from single `cl::Buffer` to `cl::Buffer [2]` arrays:
   ```cpp
   cl::Buffer point_buf_[2];
   cl::Buffer compact_buf_[2];
   cl::Buffer mask_buf_[2];
   cl::Buffer scan_buf_[2];
   cl::Buffer tile_sums_buf_[2];
   cl::Buffer accum_buf_[2];
   ```
   All existing code that referenced e.g. `point_buf_` must be updated to `point_buf_[0]` for the single-buffer path (when `use_double_buffer_` is `false`).

4. **Add new members:**
   ```cpp
   bool               use_double_buffer_ = false;
   std::atomic<int>   active_buf_{0};
   cl::Event          prev_done_ev_;   // tracks the final enqueueReadBuffer event of the in-flight dispatch
   ```

5. **Modify `run_pipeline`** to accept a buffer index parameter `int buf_idx`:
   - All buffer references use `point_buf_[buf_idx]`, etc.
   - Rename current `run_pipeline` signature to `run_pipeline(const uint8_t* data, uint32_t num_pts, uint32_t point_step, const std_msgs::msg::Header& hdr, int buf_idx)`.

6. **Blocking path** (existing behaviour, `use_double_buffer_` is `false`):
   - Always calls `run_pipeline(..., 0)` — identical to current logic.

7. **Non-blocking path** (`use_double_buffer_` is `true`):
   ```
   In subscription callback:
     write_idx = active_buf_.load()         // buffer for THIS message
     alt_idx   = 1 - write_idx             // "other" buffer (GPU is processing it)

     // Contention guard: if alt_idx == write_idx, the cl::Event callback hasn't
     // fired yet (prev dispatch not done). Fall back to blocking wait.
     // NOTE: since active_buf_ starts at 0 and only swaps on event completion,
     // detect contention by checking if prev_done_ev_ is valid and not complete.
     if (prev_done_ev_ is valid && not complete):
         queue_.finish();
         RCLCPP_WARN(..., "double-buffer contention — falling back to blocking wait");

     run_pipeline(data, num_pts, point_step, header, write_idx)
       — uses CL_FALSE (non-blocking) for enqueueWriteBuffer and final enqueueReadBuffer
       — does NOT call queue_.finish() at end
       — returns the final download cl::Event

     prev_done_ev_ = final_download_event;

     // Register atomic swap callback on final download event.
     // WHY lambda with atomic: cl::Event callback fires in driver thread —
     // only std::atomic store is safe; no RCLCPP, no mutex.
     prev_done_ev_.setCallback(CL_COMPLETE, [](cl_event, cl_int, void* user) {
         auto* ab = static_cast<std::atomic<int>*>(user);
         int cur = ab->load();
         ab->store(1 - cur);
     }, &active_buf_);
   ```

8. **Contention detection:** check whether `prev_done_ev_` is valid and its `CL_EVENT_COMMAND_EXECUTION_STATUS` is not `CL_COMPLETE` before dispatching into the same buffer index.

9. **`on_cleanup()`:** reset all 6 `[1]` buffers to `cl::Buffer()` (only if `use_double_buffer_` was true).

10. **Update `--help` text** to document the new `use_double_buffer` parameter:
    ```
    use_double_buffer  bool    default: false         — enable non-blocking double-buffer path (C3 Challenge)
    ```

---

## Definition of Done (DoD)

Standard items from `00_master_specs.md §8` apply.

- [ ] `cmake -B build && cmake --build build` succeeds with zero errors and zero warnings from `C3_Perception_Node/`.
- [ ] `./build/perception_node --help` prints the `use_double_buffer` parameter in usage.
- [ ] `GPU=<vendor> ./build/perception_node --ros-args -p use_double_buffer:=false` — behaves identically to the pre-challenge implementation (single-buffer blocking path unchanged).
- [ ] `GPU=<vendor> ./build/perception_node --ros-args -p use_double_buffer:=true -p max_points:=200000` starts without error and subscribes on `/points`.
- [ ] With `./build/point_cloud_publisher --topic /points --hz 200 --points 100000` running in parallel, the node log shows `[MSG ...]` lines with total latency, and zero `WARN: double-buffer contention` entries for at least 10 seconds under nominal load. †
- [ ] MANUAL: Launch with `use_double_buffer:=true`; open RViz; add PointCloud2 display on `/filtered_points`; confirm point cloud renders and updates at ≥ 200 Hz with no visible stalls over 10 s.
- [ ] MANUAL: Confirm zero `WARN: double-buffer contention` log lines in the node terminal during the 10 s RViz session at 200 Hz, 100k pts.

† Hardware-waiver: contention-free rate gate may not be achievable on CPU-fallback or integrated GPU. If GPU path meets the gate on the primary device, it is considered passed with a note.

---

## Execution Report

- **Status:** PENDING
- **Session:** —

### Validation
```
[output here]
```

### Changed Files
| File | Change |
|------|--------|
| `02_Projects/C_Robotics_ROS2/C3_Perception_Node/main.cpp` | Modified — add double-buffer path |
