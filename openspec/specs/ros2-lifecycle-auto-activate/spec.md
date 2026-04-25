# Spec: ros2-lifecycle-auto-activate

## Purpose
All LifecycleNodes in the lab shall support an `auto_activate` parameter that controls whether the node self-transitions to ACTIVE after configure, enabling both hands-off demo mode and explicit lifecycle management for teaching purposes.

## Requirements

### Requirement: auto_activate parameter on all LifecycleNodes
C1, C2, and C3 LifecycleNodes SHALL declare an `auto_activate` boolean parameter (default `true`) in `on_configure()`.

#### Scenario: Self-transition fires when auto_activate is true
- **WHEN** the node's `on_configure()` completes and `auto_activate` is `true`
- **THEN** the node SHALL transition to ACTIVE state via a one-shot wall timer without external intervention

#### Scenario: Node waits when auto_activate is false
- **WHEN** the node's `on_configure()` completes and `auto_activate` is `false`
- **THEN** the node SHALL remain in INACTIVE state until an external lifecycle command is received

#### Scenario: External lifecycle management works with auto_activate false
- **WHEN** a student runs `ros2 launch <pkg> <file> auto_activate:=false` and then `ros2 lifecycle set /<node_name> activate`
- **THEN** the node SHALL transition to ACTIVE state and begin processing

### Requirement: Launch files expose auto_activate as a launch argument
Each LifecycleNode launch file SHALL declare `auto_activate` as a launch argument with default `'true'` and pass it to the composable node as a parameter.

#### Scenario: Launch argument overrides parameter default
- **WHEN** a student runs `ros2 launch <pkg> <file> auto_activate:=false`
- **THEN** the node SHALL start in INACTIVE state after configure
