## ADDED Requirements

### Requirement: Recording and Replay section in each README
PerceptionNode.md, VoxelMapping.md, and NodeAcceleration.md SHALL each include a "Recording & Replay" section with commands for `ros2 bag record`, `ros2 bag info`, and `ros2 bag play`.

#### Scenario: C3 and Voxel Mapping show publisher substitution workflow
- **WHEN** a student reads the "Recording & Replay" section of PerceptionNode.md or VoxelMapping.md
- **THEN** the section SHALL explain how to stop the synthetic publisher and replay a bag on the same topic as a drop-in replacement

#### Scenario: NodeAcceleration.md documents the intra-process constraint
- **WHEN** a student reads the "Recording & Replay" section of NodeAcceleration.md
- **THEN** the section SHALL note that bag-driven input is not applicable due to the intra-process architecture, and show recording `/processed_floats` output instead

#### Scenario: Rate flag documented
- **WHEN** the "Recording & Replay" section is read
- **THEN** the `--rate` flag for `ros2 bag play` SHALL be mentioned with an example
