## ADDED Requirements

### Requirement: Launch files accept optional bag path to replace synthetic publisher
All three launch files (`node_acceleration.launch.py`, `perception_node.launch.py`, `voxel_mapping.launch.py`) SHALL accept a `bag` launch argument (default `''`). When `bag` is set to a non-empty path, the synthetic publisher composable node SHALL be omitted from the container and `ros2 bag play <path>` SHALL be started as a side process. When `bag` is empty, behaviour SHALL be identical to the pre-change default.

#### Scenario: Default launch (no bag) — publisher included
- **WHEN** a launch file is invoked without the `bag` argument
- **THEN** the synthetic publisher composable node is present in the container and the processing node receives messages from it as before

#### Scenario: Bag path provided — publisher suppressed, bag plays
- **WHEN** a launch file is invoked with `bag:=/path/to/bag`
- **THEN** the synthetic publisher composable node is absent from the container AND `ros2 bag play /path/to/bag` runs as a side process publishing to the same topic

#### Scenario: Bag plays to correct topic without configuration
- **WHEN** `bag:=<path>` is set and the bag contains messages on the node's input topic
- **THEN** the processing node receives and processes those messages without additional configuration

#### Scenario: --help shows bag argument
- **WHEN** `ros2 launch <package> <launch_file> --show-args` is run
- **THEN** `bag` appears in the list with its description
