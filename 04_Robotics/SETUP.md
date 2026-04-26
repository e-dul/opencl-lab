# ROS 2 Setup — Ubuntu 24.04 (Jazzy)

## 1. Install ROS 2

### Add locale
```bash
sudo apt update && sudo apt install -y locales
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
```

### Add APT repository
```bash
sudo apt install -y software-properties-common curl
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
    -o /usr/share/keyrings/ros-archive-keyring.gpg

echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] \
    http://packages.ros.org/ros2/ubuntu noble main" \
    | sudo tee /etc/apt/sources.list.d/ros2.list

sudo apt update
```

### Install
```bash
sudo apt install -y ros-jazzy-desktop \
    ros-jazzy-rclcpp ros-jazzy-sensor-msgs ros-jazzy-nav-msgs \
    ros-jazzy-rmw-fastrtps-cpp ros-jazzy-demo-nodes-cpp \
    ros-jazzy-ros2bag ros-jazzy-rosbag2-transport \
    ros-jazzy-diagnostic-updater \
    python3-colcon-common-extensions
```

---

## 2. Source the Workspace

**Must precede every `cmake` configure step** — without it `find_package(rclcpp)` fails silently.

```bash
source /opt/ros/jazzy/setup.bash
```

Persist in shell:
```bash
echo "source /opt/ros/jazzy/setup.bash" >> ~/.bashrc
```

---

## 3. Loaned Messages (C3 optimal performance)

The default RMW (CycloneDDS) does not support loaned messages. Switch to FastDDS:

```bash
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
```

Without this, C3 falls back to copy-based transport and prints a warning. The 5 ms latency gate may not be achievable on the standard path.

---

## 4. Verify

```bash
env | grep ROS
# Expected: ROS_DISTRO=jazzy

ros2 run demo_nodes_cpp talker &
ros2 run demo_nodes_cpp listener
# Expected: listener prints "Hello World: N" at ~1 Hz

ros2 doctor
# Expected: all checks pass (or only warnings for optional features)
```

---

## Notes

- **C1, C2, C3, and 4.5 Voxel Mapping** all require a sourced ROS 2 workspace before `colcon build`. C1 and C2 use `rclcpp_lifecycle::LifecycleNode` and therefore depend on `find_package(rclcpp_lifecycle REQUIRED)`.
- RMW fallback is explicit and logged — never a silent failure or crash.
- **`colcon build` working directory**: run from within the package directory, or from a parent directory with `--packages-select`. Do **not** run from the repo root — old `build/` subdirectories left by standalone cmake builds contain `AMENT_IGNORE` files that prevent colcon from discovering the packages.

  ```bash
  # All C-track packages at once:
  cd ~/opencl-lab/04_Robotics
  colcon build --packages-select node_acceleration costmap_inflation perception_node

  # Voxel Mapping:
  cd ~/opencl-lab/06_Bonus/04_Voxel_Mapping
  colcon build
  ```

---

## 5. Recording & Replay

`ros2 bag` captures any topic to disk and replays it in place of a live publisher.

**Record** while the node is running:

```bash
ros2 bag record <topic> -o <bag_name>
```

**Inspect:**

```bash
ros2 bag info <bag_name>
```

**Replay** — stop the publisher, keep the node running, then:

```bash
ros2 bag play <bag_name>
ros2 bag play --rate 0.5 <bag_name>   # half speed
ros2 bag play --rate 2.0 <bag_name>   # double speed
```

**QoS note**: `ros2 bag play` publishes with the QoS stored in the bag metadata. If the stored QoS (`Reliable`) mismatches the node subscription (`BestEffort`), messages are silently dropped. Override with:

```bash
ros2 bag play --qos-profile-overrides-path qos_override.yaml <bag_name>
```

---

[Back to RoboticsROS2.md](RoboticsROS2.md)
