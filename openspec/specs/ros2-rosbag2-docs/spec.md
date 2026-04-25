## Purpose

Defines the documentation requirements for `ros2 bag` record/replay workflows across the ROS 2 modules (C1, C3, Voxel Mapping).

## Requirements

### Requirement: Canonical Recording & Replay workflow lives in SETUP.md
`04_Robotics/SETUP.md` SHALL contain a "Recording & Replay" section (§5) with the generic record → inspect → replay command sequence, `--rate` examples, and QoS override instructions. This is the single source of truth; module READMEs link to it rather than duplicating the workflow.

#### Scenario: SETUP.md §5 covers full workflow
- **WHEN** a student reads `04_Robotics/SETUP.md`
- **THEN** §5 SHALL show `ros2 bag record <topic> -o <bag_name>`, `ros2 bag info <bag_name>`, `ros2 bag play <bag_name>`, `--rate` examples, and the `--qos-profile-overrides-path` override note

#### Scenario: rosbag2 packages declared in SETUP.md install block
- **WHEN** a student follows the §1 install command in SETUP.md
- **THEN** `ros-jazzy-ros2bag` and `ros-jazzy-rosbag2-transport` SHALL be included in the `apt install` block so no per-module install step is needed

### Requirement: Module READMEs include a Recording & Replay stub linking to SETUP.md
`PerceptionNode.md`, `VoxelMapping.md`, and `NodeAcceleration.md` SHALL each contain a "## Recording & Replay" section that states the module-specific topic and bag name, then links to `SETUP.md §5` for the full workflow. The full command sequence SHALL NOT be duplicated in module READMEs.

#### Scenario: PerceptionNode.md stub is correct
- **WHEN** a student reads the "Recording & Replay" section of PerceptionNode.md
- **THEN** the section SHALL state topic `/points`, bag name `perception_bag`, and link to `../SETUP.md#5-recording--replay`

#### Scenario: VoxelMapping.md stub is correct
- **WHEN** a student reads the "Recording & Replay" section of VoxelMapping.md
- **THEN** the section SHALL state topic `/points`, bag name `voxel_bag`, and link to `../../04_Robotics/SETUP.md#5-recording--replay`

#### Scenario: NodeAcceleration.md stub documents intra-process constraint
- **WHEN** a student reads the "Recording & Replay" section of NodeAcceleration.md
- **THEN** the section SHALL note that the intra-process channel cannot be driven by a bag, identify `/processed_floats` as the recordable output topic, bag name `accel_bag`, and link to `../SETUP.md#5-recording--replay`
