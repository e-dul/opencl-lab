## 1. PerceptionNode.md — Recording & Replay Section

- [ ] 1.1 Read `04_Robotics/03_Perception_Node/PerceptionNode.md` in full to identify insertion point (after Build & Run / before Troubleshooting)
- [ ] 1.2 Add "Recording & Replay" section with: (a) `ros2 bag record /points -o perception_bag` while publisher runs; (b) `ros2 bag info perception_bag`; (c) stop publisher, keep `perception_node` running, then `ros2 bag play perception_bag`; (d) `--rate 0.5` / `--rate 2.0` examples; (e) one-line apt install note for `ros-jazzy-ros2bag`; (f) QoS troubleshooting note referencing `--qos-profile-overrides-path`

## 2. VoxelMapping.md — Recording & Replay Section

- [ ] 2.1 Read `06_Bonus/04_Voxel_Mapping/VoxelMapping.md` in full to identify insertion point
- [ ] 2.2 Add "Recording & Replay" section with: (a) `ros2 bag record /points -o voxel_bag`; (b) `ros2 bag info voxel_bag`; (c) stop publisher, keep `voxel_mapping` running, then `ros2 bag play voxel_bag`; (d) `--rate` examples; (e) apt install note; (f) QoS troubleshooting note

## 3. NodeAcceleration.md — Recording & Replay Section

- [ ] 3.1 Read `04_Robotics/01_Node_Acceleration/NodeAcceleration.md` in full to identify insertion point
- [ ] 3.2 Add "Recording & Replay" section explaining: (a) intra-process constraint — bag cannot drive `accel_node` directly; (b) record `/processed_floats` output: `ros2 bag record /processed_floats -o accel_bag`; (c) `ros2 bag info accel_bag`; (d) `ros2 bag play accel_bag` to replay output for downstream consumers or analysis; (e) apt install note
