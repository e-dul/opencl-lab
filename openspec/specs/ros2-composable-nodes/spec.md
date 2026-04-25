# Spec: ros2-composable-nodes

## Purpose
All ROS 2 node classes shall be registered as composable components to enable runtime composition and intra-process communication, eliminating DDS serialisation overhead when nodes share a container.

## Requirements

### Requirement: All ROS 2 node classes registered as composable components
Every node class in the four modules SHALL be registered via `RCLCPP_COMPONENTS_REGISTER_NODE` and buildable as a shared library via `add_library(SHARED)` + `rclcpp_components_register_node()`.

#### Scenario: Component loadable at runtime
- **WHEN** a student runs `ros2 component load /ComponentManager <package> <namespace::ClassName>`
- **THEN** the component SHALL load successfully and the node SHALL appear in `ros2 node list`

#### Scenario: Standalone executable wrapper available
- **WHEN** `rclcpp_components_register_node` is called with the `EXECUTABLE` argument
- **THEN** a standalone executable SHALL be generated that runs the node in a single-node container

### Requirement: Companion publishers are composable components in the same package
Each companion publisher binary (`synthetic_publisher`, `map_publisher`, `point_cloud_publisher`, `voxel_point_cloud_publisher`) SHALL be registered as a composable component in the same package as its paired node.

#### Scenario: Companion loaded into same container
- **WHEN** the launch file starts a `ComposableNodeContainer`
- **THEN** both the main node and its companion publisher SHALL be loaded into the same container

### Requirement: Intra-process communication enabled in launch containers
All `ComposableNodeContainer` instances in launch files SHALL set `use_intra_process_comms: True` for all composable node descriptions.

#### Scenario: Zero-copy delivery confirmed
- **WHEN** the main node and companion publisher run in the same container with intra-process enabled
- **THEN** messages SHALL be delivered without DDS serialisation (verifiable via `ros2 topic info --verbose` showing no DDS transport for intra-process topics)

### Requirement: Companion publisher parameters via declare_parameter
All companion publisher binaries that previously used CLI11 for argument parsing SHALL use `declare_parameter` and `get_parameter` instead.

#### Scenario: Publisher parameters settable via launch args
- **WHEN** a student runs `ros2 launch <package> <file> hz:=10 scene:=mixed`
- **THEN** the companion publisher SHALL use those parameter values
