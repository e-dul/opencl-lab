## Context

C3 PerceptionNode and Voxel Mapping currently pass bare integers to `create_publisher` and `create_subscription`. This implicitly selects `KeepLast(N)`, `Reliable`, `Volatile` — the ROS 2 defaults. For sensor data topics, the standard profile is `SensorDataQoS` (`BestEffort`, `Volatile`, `KeepLast(5)`). A QoS mismatch between publisher and subscriber produces a silent no-connection: `ros2 topic list` shows both sides active, but no messages flow.

C2 already demonstrates the correct pattern for latched map topics (`rclcpp::QoS(1).transient_local()`). This change completes the picture for sensor stream topics.

## Goals / Non-Goals

**Goals:**
- Replace bare integer QoS in C3 and Voxel Mapping with named profiles
- Add one-line WHY comment + ROS 2 QoS docs link on each pub/sub call
- Make the QoS choice explicit and copy-paste-safe for students

**Non-Goals:**
- C1 Node Acceleration (intra-process; DDS QoS is irrelevant)
- C2 Costmap Inflation (already correct)
- CMakeLists.txt changes (no new dependencies)
- Kernel or launch file changes
- Deep QoS tutorial in code comments (link suffices)

## Decisions

### D1: SensorDataQoS for PointCloud2 sensor stream topics

**Decision**: Use `rclcpp::SensorDataQoS()` for:
- C3: `/filtered_points` publisher, `/cluster_features` publisher, input topic subscription
- Voxel Mapping: input topic subscription

**Why**: `SensorDataQoS` (`BestEffort`, `Volatile`, `KeepLast(5)`) matches what sensor drivers (Velodyne, Ouster, RealSense) publish. A `Reliable` subscriber against a `BestEffort` publisher is the silent no-connection footgun. Using the named profile makes the intent self-documenting and avoids the mismatch.

**Alternative considered**: `rclcpp::QoS(rclcpp::KeepLast(10)).best_effort()` — identical semantics but verbose. `SensorDataQoS()` is the idiomatic shorthand.

### D2: KeepLast(1) for Voxel Mapping output publishers

**Decision**: Use `rclcpp::QoS(rclcpp::KeepLast(1))` (Reliable, Volatile) for `/voxel_map` and `/voxel_slice`.

**Why**: These are computed output artifacts — consumers (RViz2, rosbag2) expect reliable delivery of the latest result. `BestEffort` would risk losing the only published message in a low-rate stream. `KeepLast(1)` keeps memory bounded. The bare `1` that exists today happens to be correct in depth but leaves reliability implicit.

**Alternative considered**: `SensorDataQoS()` — rejected because output topics are not sensor data; subscribers expect Reliable delivery of computed results.

### D3: Comment format

**Decision**: One-line comment above each call:
```cpp
// SensorDataQoS: BestEffort+Volatile matches sensor drivers; Reliable sub ↔ BestEffort pub = silent no-connection
// Ref: https://docs.ros.org/en/jazzy/Concepts/Intermediate/About-Quality-of-Service-Settings.html
```

**Why**: Two lines is the minimum to explain both the policy choice and the footgun. Avoids bloating the code with a QoS tutorial.

## Risks / Trade-offs

- **Existing rosbag2 recordings**: If a student has recorded `/filtered_points` with the old Reliable QoS, replaying against the new BestEffort subscription will succeed (BestEffort sub accepts Reliable pub). No breakage.
- **Third-party subscribers**: A student-written node subscribing to `/filtered_points` with `Reliable` default will lose connection after this change. → Mitigation: the comment makes the required QoS visible at the call site.
