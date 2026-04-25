## Context

All three launch files use `ComposableNodeContainer` with both the processing node and its synthetic publisher as composable nodes in the same container (`use_intra_process_comms=True`). `ComposableNode` descriptions do not accept a `condition=` argument — only top-level `Action` objects do. This means the publisher cannot be conditionally excluded with a simple `IfCondition`.

When `bag` is set, the synthetic publisher must be omitted from the container and replaced with an `ExecuteProcess` action that runs `ros2 bag play <path>`. The bag publishes via DDS; the processing node receives messages on the same topic regardless of whether they arrived intra-process or over DDS.

**C1 note**: the existing Recording & Replay docs say "intra-process constraint — record output only." That constraint disappears with this change: when the publisher is removed from the container, `/raw_floats` input can be replayed from a bag over DDS. The docs must be updated to reflect this.

## Goals / Non-Goals

**Goals:**
- Add `bag` launch arg (default `''`) to all three launch files; empty = no change to existing behaviour.
- When `bag` is non-empty: drop the synthetic publisher from the container and start `ros2 bag play <path>` as a side process.
- Update NodeAcceleration.md Recording & Replay stub to remove the now-obsolete intra-process constraint note.
- Update PerceptionNode.md and VoxelMapping.md stubs with the one-liner example.

**Non-Goals:**
- Bag recording from within the launch file.
- `--rate`, `--loop`, or other `ros2 bag play` options as launch args (user passes them via the bag path string or overrides manually).
- Any node source code changes.

## Decisions

### D1: `OpaqueFunction` for conditional composable node list

**Decision**: Use `OpaqueFunction` to build the composable node descriptions and side actions at launch time, based on the resolved value of the `bag` argument.

**Why**: `ComposableNode` has no `condition=` support, so the only clean options are:
- *Two full container definitions* with `IfCondition`/`UnlessCondition` — duplicates the entire container block.
- *`OpaqueFunction`* — evaluates `bag` at launch time, builds the composable list and action list with ordinary Python `if/else`, then returns them.

`OpaqueFunction` is more concise and keeps the container definition DRY.

**Pattern** (same for all three files):
```python
from launch.actions import ExecuteProcess, OpaqueFunction

def launch_setup(context, *args, **kwargs):
    bag_path = LaunchConfiguration('bag').perform(context)
    publisher_node = ComposableNode(...)   # synthetic publisher
    node = ComposableNode(...)             # processing node

    nodes = [node] if bag_path else [node, publisher_node]
    actions = [ComposableNodeContainer(..., composable_node_descriptions=nodes)]
    if bag_path:
        actions.append(ExecuteProcess(
            cmd=['ros2', 'bag', 'play', bag_path],
            output='screen',
        ))
    return actions

return LaunchDescription([
    DeclareLaunchArgument('bag', default_value='',
                          description='Path to a rosbag2 bag directory; '
                                      'suppresses the synthetic publisher when set.'),
    OpaqueFunction(function=launch_setup),
])
```

### D2: `bag` arg carries the full path — no extra play options

**Decision**: The `bag` argument is passed verbatim to `ros2 bag play`. No `--rate`, `--loop`, or `--qos-profile-overrides-path` launch args are added.

**Why**: Power users can run `ros2 bag play` manually for full control. Adding each play option as a launch arg is boilerplate with low pedagogical value. The launch integration is a convenience entry point, not a full bag-player wrapper.

### D3: C1 intra-process docs updated

**Decision**: NodeAcceleration.md Recording & Replay stub is rewritten to remove the intra-process constraint and show the full record → replay cycle for `/raw_floats`.

**Why**: The constraint was a consequence of the publisher being inseparable from the node. With `bag:=<path>` the publisher is dropped, so DDS-delivered messages on `/raw_floats` reach `accel_node` normally.

## Risks / Trade-offs

- **`ros2 bag play` exit does not stop the node**: when the bag finishes, `accel_node` stays running (idle). User must Ctrl-C. This is acceptable — consistent with how ROS 2 nodes behave when their publisher disappears.
- **QoS mismatch**: bags recorded before `ros2-qos-profiles` lands carry `Reliable` QoS; nodes post-change use `BestEffort`. Messages will be silently dropped. Mitigation: the SETUP.md QoS note already covers `--qos-profile-overrides-path`.
- **`OpaqueFunction` opacity**: launch argument substitutions inside `OpaqueFunction` are resolved via `.perform(context)`, which bypasses the declarative substitution graph. This is standard ROS 2 practice but can surprise readers. Mitigate with a `# WHY OpaqueFunction` comment in the launch file.
