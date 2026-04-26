## Context

Both nodes accumulate GPU timing per message but currently discard the window — only the latest message's timing is visible. The rolling accumulator keeps a fixed-size circular buffer of `total_ms` values and recomputes `min/max/avg` on each diagnostic tick (1 Hz). The diagnostic callback captures a snapshot of the current member variables at tick time; no locking is needed because `SingleThreadedExecutor` serialises the subscription callback (which writes the accumulator and member vars) and the diagnostic timer callback (which reads them).

C3 uses `rclcpp_lifecycle::LifecycleNode`; the `diagnostic_updater::Updater` is initialised in `on_configure()` and the diagnostic tasks are added there. Voxel Mapping uses a plain `rclcpp::Node`; the updater is initialised in the constructor.

## Goals / Non-Goals

**Goals:**
- Replace per-message terminal noise with a 1 Hz `/diagnostics` report
- Accumulate `min/max/avg` GPU kernel time over a 100-message rolling window
- Expose `GPU pipeline` and `Config` diagnostic tasks per node
- Encode a soft performance gate in the status bit (OK/WARN/ERROR)
- `rqt_robot_monitor` displays both nodes' health without any configuration

**Non-Goals:**
- C1 or C2 changes
- `diagnostic_updater::HeaderlessTopicDiagnostic` frequency monitoring (out of scope)
- Per-stage breakdown in the diagnostic (total pipeline time only; stage detail remains in on_configure RCLCPP_INFO init log)
- Persisting diagnostic history

## Decisions

### D1: Rolling accumulator size = 100 messages

**Decision**: Circular buffer of 100 `total_ms` samples. `min/max/avg` recomputed on each 1 Hz tick.

**Why**: At 200 Hz input, 100 samples covers ~0.5 s — enough to smooth single-frame outliers without masking a sustained regression. At 10 Hz (Voxel Mapping default), 100 samples = 10 s window, which remains responsive to parameter changes.

**Alternative considered**: Exponential moving average — simpler but `min/max` are undefined. Circular buffer gives all three metrics cleanly.

### D2: Performance gate thresholds

**C3 PerceptionNode** (four-stage pipeline, 100k points):
- `OK`: avg_total_ms ≤ 10 ms
- `WARN`: avg_total_ms ≤ 25 ms — `"Pipeline slower than expected"`
- `ERROR`: avg_total_ms > 25 ms — `"Pipeline stalled"`

**Voxel Mapping** (DDA + optional flip, 100k points):
- `OK`: avg_total_ms ≤ 5 ms (matches existing `[WARN]` gate in the code)
- `WARN`: avg_total_ms ≤ 15 ms — `"Pipeline exceeds 5 ms gate"`
- `ERROR`: avg_total_ms > 15 ms — `"Pipeline stalled"`

**Why**: C3's four stages justify a looser gate than Voxel Mapping's two. Both preserve the existing performance expectations documented in the module comments.

### D3: Voxels_occupied metric source

**Decision**: Read `voxels_occupied_` count that is updated inside the DDA callback after each frame. Expose as `voxels_occupied` and `total_voxels` (grid dims product) in the diagnostic.

**Why**: Occupancy ratio tells the user immediately if the scene is too sparse (resolution too coarse) or the grid is saturating. No additional GPU readback needed — the host already has the grid dimensions.

### D4: Remove std::cout from Voxel Mapping; keep RCLCPP_INFO for init/shutdown

**Decision**: Replace the per-message `std::cout` timing block entirely. Retain existing `RCLCPP_INFO` calls for one-time events (node init, kernel compile time, shutdown summary).

**Why**: `std::cout` in a ROS 2 node bypasses the logger and is invisible to `ros2 launch` log routing. The per-message timing was the only use; removing it cleans the node without losing any structural logging.

### D5: Updater hardware_id

**Decision**: Set `updater_.setHardwareID("none")` for both nodes.

**Why**: These are software pipeline nodes with no physical hardware identity. `"none"` is the conventional value for pure-software diagnostic producers.

### D6: Config diagnostic — parameter snapshot approach

**Decision**: The `Config` diagnostic callback reads member variables directly (`ground_z_`, `min_intensity_`, etc.), not `get_parameter()`. Add each as a key-value pair via `stat.add()`. Status always `OK`.

**Why**: Member variables are the source of truth for what the pipeline actually used on the last message. Reading them is safe on `SingleThreadedExecutor`. `get_parameter()` would add unnecessary IPC overhead and could diverge if a param callback rejection left a mismatch.

## Risks / Trade-offs

- **Accumulator before first tick**: On the first 1 Hz tick the window may have fewer than 100 samples. Report actual sample count; `min/max/avg` computed over available samples — no divide-by-zero guard needed (tick fires at 1 Hz, first message arrives immediately).
- **SingleThreadedExecutor assumption**: If executor is ever changed to multi-threaded, the accumulator write (in subscription callback) and read (in diagnostic timer) would race. → Add a WHY comment documenting the assumption (consistent with the pattern established in `ros2-param-improvements` D3).
- **diagnostic_updater CMake change**: Both CMakeLists.txt gain a new `find_package`. On a system without the `diagnostic_updater` package, the build fails with a clear CMake error rather than a linker error — acceptable.
