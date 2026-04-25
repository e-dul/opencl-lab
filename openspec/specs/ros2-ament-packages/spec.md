# Spec: ros2-ament-packages

## Purpose
Each ROS 2 lab module shall be a properly structured ament_cmake package, discoverable via `ros2 pkg list` after a colcon build, with kernels installed to the package share directory.

## Requirements

### Requirement: Each ROS 2 module is a discoverable ament_cmake package
`01_Node_Acceleration`, `02_Costmap_Inflation`, `03_Perception_Node`, and `06_Bonus/04_Voxel_Mapping` SHALL each contain a `package.xml` and call `ament_package()` in their `CMakeLists.txt`.

#### Scenario: Package is discoverable after colcon build
- **WHEN** a student runs `colcon build` from within a module directory and `source install/setup.bash`
- **THEN** `ros2 pkg list` SHALL include the package name

#### Scenario: Launch file works by package name
- **WHEN** a student runs `ros2 launch <package_name> <launch_file>`
- **THEN** the launch file SHALL be found and executed without specifying a filesystem path

### Requirement: Each module builds in isolation
Each ROS 2 module SHALL be buildable by running `colcon build` from within its own directory, with no dependency on other lab modules being built or present.

#### Scenario: Standalone colcon build succeeds
- **WHEN** a student runs `colcon build` from `04_Robotics/01_Node_Acceleration/` with no other lab packages present
- **THEN** the build SHALL succeed with zero errors and zero warnings

### Requirement: Kernel files installed to package share directory
Each module's `kernels/` directory SHALL be installed to `share/<package_name>/kernels/` so kernels are accessible after `source install/setup.bash`.

#### Scenario: Kernels accessible from installed tree
- **WHEN** a node is launched via `ros2 launch` after `source install/setup.bash`
- **THEN** the node SHALL locate and load its OpenCL kernel files without error
