## Context

Each module pairs a node-under-test with a synthetic publisher. The publisher and node run in separate terminals (or via launch file). Replacing the publisher with `ros2 bag play` requires only stopping the publisher — the node keeps running, subscribed to the same topic. The bag replays at the recorded rate by default; `--rate` scales it.

C1 is a special case: its publisher and node run in the **same binary** (single-process, intra-process comms). A bag replay would require running the node separately and subscribing externally — this breaks the intra-process design. The section for C1 notes this constraint and limits the workflow to recording `/processed_floats` output for observation rather than driving the node from a bag.

## Goals / Non-Goals

**Goals:**
- Show the record → inspect → replay loop in plain shell commands
- Explain how to substitute bag replay for the synthetic publisher (C3, Voxel Mapping)
- Note the `--rate` flag for faster/slower replay
- Note C1's intra-process constraint

**Non-Goals:**
- rosbag2 plugin configuration or storage format selection
- Filtering by multiple topics or time ranges
- Programmatic bag access (Python/C++ API)
- Any code changes

## Decisions

### D1: Section placement

**Decision**: Add "Recording & Replay" as a new top-level section in each README, after the existing "Run" or "Build & Run" section and before "Troubleshooting" (if present).

**Why**: It is a natural extension of the run workflow — a student reads how to run, then immediately sees how to capture and replay. Placing it before Troubleshooting keeps the happy-path narrative intact.

### D2: C1 — record output only, not input

**Decision**: For NodeAcceleration.md, show recording `/processed_floats` (the node's output topic) as an observation tool. Do not present bag replay as a way to drive `accel_node`, because its input arrives via intra-process from `synthetic_publisher` in the same binary.

**Why**: Presenting bag-driven input for C1 would require refactoring the binary into a separately launchable node, which changes the architecture lesson. The output-recording use case is still valuable (capture a run for later analysis).

### D3: Command style — plain `ros2 bag` commands, no launch integration

**Decision**: Show standalone `ros2 bag` commands, not a launch action. No launch file change.

**Why**: The launch file approach (`ExecuteProcess` with `ros2 bag play`) adds complexity without teaching benefit at this stage. The manual workflow is clearer and sufficient.

## Risks / Trade-offs

- **ros-jazzy-ros2bag package**: `ros2 bag` requires `ros-jazzy-ros2bag` (or equivalent). On a minimal ROS install it may not be present. → Note a one-line install hint in each section.
- **Bag replay QoS**: `ros2 bag play` publishes with the QoS stored in the bag metadata. After `ros2-qos-profiles` is applied, the stored QoS will be `BestEffort`; the node subscription will also be `BestEffort`. If a bag was recorded before that change, the stored QoS may be `Reliable`. `ros2 bag play --qos-profile-overrides-path` can override — mention as a troubleshooting note.
