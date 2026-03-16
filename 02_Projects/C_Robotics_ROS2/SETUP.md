# ROS 2 Setup — Ubuntu 24.04 (Jazzy)

**Status:** Draft

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
    ros-jazzy-rmw-fastrtps-cpp
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
ros2 --version
# Expected: ros2 cli 0.22.x

ros2 run demo_nodes_cpp talker &
ros2 run demo_nodes_cpp listener
# Expected: listener prints "Hello World: N" at ~1 Hz
```

---

## Notes

- **C1 and C3** require a sourced workspace before `cmake -B build`.
- **C2** has no ROS 2 dependency — skip sections 1–3 if only running C2.
- RMW fallback is explicit and logged — never a silent failure or crash.
