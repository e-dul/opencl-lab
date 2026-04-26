# Spec: ros2-diagnostics

## Purpose

ROS 2 diagnostic integration for C3 PerceptionNode and Voxel Mapping. Nodes expose GPU pipeline performance metrics and live configuration via `diagnostic_updater::Updater`, replacing per-message terminal output with structured diagnostics queryable at runtime.

## Requirements

### Requirement: Rolling timing accumulator
C3 PerceptionNode and Voxel Mapping SHALL maintain a circular buffer of the 100 most recent total GPU pipeline durations (in milliseconds). The buffer SHALL be updated inside the message subscription callback after every processed frame.

#### Scenario: Accumulator updates on each message
- **WHEN** a PointCloud2 message is processed and GPU timing is available
- **THEN** the total pipeline duration SHALL be written into the circular buffer, advancing the write index modulo 100

#### Scenario: Accumulator computes min/max/avg over available samples
- **WHEN** the diagnostic callback fires and fewer than 100 samples have been collected
- **THEN** min/max/avg SHALL be computed over the actual number of samples collected so far (no divide-by-zero, no default zero-fill)

### Requirement: GPU pipeline diagnostic task
Both C3 PerceptionNode and Voxel Mapping SHALL register a diagnostic task named `"GPU pipeline"` with `diagnostic_updater::Updater`. The task SHALL report: `avg_kernel_ms`, `min_kernel_ms`, `max_kernel_ms`, `messages_processed`, and node-specific throughput metrics.

#### Scenario: C3 GPU pipeline diagnostic fields
- **WHEN** the `"GPU pipeline"` diagnostic task fires for C3 PerceptionNode
- **THEN** the status SHALL include `avg_kernel_ms`, `min_kernel_ms`, `max_kernel_ms`, `messages_processed`, `points_in_avg`, `points_out_avg`

#### Scenario: Voxel Mapping GPU pipeline diagnostic fields
- **WHEN** the `"GPU pipeline"` diagnostic task fires for Voxel Mapping
- **THEN** the status SHALL include `avg_kernel_ms`, `min_kernel_ms`, `max_kernel_ms`, `messages_processed`, `points_in_avg`, `voxels_occupied`, `total_voxels`

### Requirement: Performance gate encoded in diagnostic status
The `"GPU pipeline"` diagnostic task SHALL set the status level based on avg_kernel_ms:
- C3: OK ≤ 10 ms, WARN ≤ 25 ms, ERROR > 25 ms
- Voxel Mapping: OK ≤ 5 ms, WARN ≤ 15 ms, ERROR > 15 ms

#### Scenario: C3 OK status
- **WHEN** C3 avg_kernel_ms ≤ 10.0
- **THEN** diagnostic status SHALL be `OK`

#### Scenario: C3 WARN status
- **WHEN** C3 avg_kernel_ms is between 10.0 and 25.0 (exclusive)
- **THEN** diagnostic status SHALL be `WARN` with message `"Pipeline slower than expected"`

#### Scenario: C3 ERROR status
- **WHEN** C3 avg_kernel_ms > 25.0
- **THEN** diagnostic status SHALL be `ERROR` with message `"Pipeline stalled"`

#### Scenario: Voxel Mapping OK status
- **WHEN** Voxel Mapping avg_kernel_ms ≤ 5.0
- **THEN** diagnostic status SHALL be `OK`

#### Scenario: Voxel Mapping WARN status
- **WHEN** Voxel Mapping avg_kernel_ms is between 5.0 and 15.0 (exclusive)
- **THEN** diagnostic status SHALL be `WARN` with message `"Pipeline exceeds 5 ms gate"`

#### Scenario: Voxel Mapping ERROR status
- **WHEN** Voxel Mapping avg_kernel_ms > 15.0
- **THEN** diagnostic status SHALL be `ERROR` with message `"Pipeline stalled"`

### Requirement: Config diagnostic task
Both nodes SHALL register a diagnostic task named `"Config"` that reports current live parameter values as key-value pairs. Status SHALL always be `OK`.

#### Scenario: C3 Config fields
- **WHEN** the `"Config"` diagnostic task fires for C3 PerceptionNode
- **THEN** the status SHALL include `ground_z`, `min_intensity`, `max_points`, `double_buffer`, `rmw` (RMW implementation string)

#### Scenario: Voxel Mapping Config fields
- **WHEN** the `"Config"` diagnostic task fires for Voxel Mapping
- **THEN** the status SHALL include `resolution`, `flip_filter`, `flip_threshold`, `topic`

### Requirement: Per-message terminal timing output removed
C3 PerceptionNode SHALL NOT emit a `RCLCPP_INFO` timing table per processed message. Voxel Mapping SHALL NOT emit a `std::cout` timing block per processed message. One-time init and shutdown log lines are unaffected.

#### Scenario: Terminal remains readable at 200 Hz
- **WHEN** C3 or Voxel Mapping processes messages at 200 Hz
- **THEN** no timing output SHALL appear in the terminal between diagnostic ticks
