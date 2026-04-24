## Why

C3 PerceptionNode and Voxel Mapping pass bare integer depth values to `create_publisher` and `create_subscription`, leaving QoS policy (reliability, durability, history) implicit. A publisher using `BestEffort` and a subscriber using the default `Reliable` produce a silent no-connection — the most common ROS 2 integration bug, invisible in `ros2 topic list` or node logs.

## What Changes

- **C3 PerceptionNode**: Replace `create_publisher(..., 10)` and `create_subscription(..., 10)` with `rclcpp::SensorDataQoS()` on all PointCloud2 publishers and the input subscription. Add WHY comment + link to ROS 2 QoS documentation on each call.
- **Voxel Mapping**: Replace bare `1` and `10` depth arguments with `rclcpp::SensorDataQoS()` on the input subscription; use `rclcpp::QoS(rclcpp::KeepLast(1))` on `/voxel_map` and `/voxel_slice` publishers (output artifacts, not sensor streams). Add WHY comment + link on each.
- **C1 Node Acceleration**: No change — pub/sub is intra-process; DDS QoS is bypassed.
- **C2 Costmap Inflation**: No change — already uses `rclcpp::QoS(1).transient_local()` correctly.

## Capabilities

### New Capabilities

- `ros2-qos-profiles`: Explicit, named QoS profiles on all ROS 2 pub/sub calls in C3 and Voxel Mapping, with rationale comments and external reference link.

### Modified Capabilities

*(none — no existing spec-level requirements change)*

## Impact

- **Files changed**: `04_Robotics/03_Perception_Node/main.cpp`, `06_Bonus/04_Voxel_Mapping/main.cpp`
- **No additional CMakeLists.txt changes**: `rclcpp::SensorDataQoS` is part of `rclcpp`; no new package dependencies
- **No kernel changes**, **no README changes**
- **No C1 or C2 changes**
- **Depends on**: `ros2-ament-migration` applied first — QoS call sites are in the same files being restructured by the migration; applying after avoids double-touching the same code
