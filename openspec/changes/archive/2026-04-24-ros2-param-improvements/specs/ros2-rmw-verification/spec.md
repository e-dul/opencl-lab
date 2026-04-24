## ADDED Requirements

### Requirement: RMW environment logged at activation
C3 PerceptionNode SHALL read the `RMW_IMPLEMENTATION` environment variable in `on_activate()` and log its value via `RCLCPP_INFO` before attempting to create the subscription.

#### Scenario: RMW name appears in log at activation
- **WHEN** the node transitions to ACTIVE
- **THEN** the log contains a line showing the active RMW implementation name (e.g., `rmw_fastrtps_cpp`)

#### Scenario: RMW unset logs fallback message
- **WHEN** `RMW_IMPLEMENTATION` is not set in the environment
- **THEN** the log shows "(default RMW)" or equivalent

### Requirement: RCLCPP_WARN emitted when RMW does not support loaned messages
C3 PerceptionNode SHALL emit `RCLCPP_WARN` in `on_activate()` when `RMW_IMPLEMENTATION` is not one of `rmw_fastrtps_cpp` or `rmw_iceoryx_cpp`, informing the user that the 5 ms performance gate may not be achievable.

#### Scenario: Warning emitted for cyclonedds
- **WHEN** `RMW_IMPLEMENTATION=rmw_cyclonedds_cpp` is set and the node activates
- **THEN** a WARN log is emitted stating loaned messages are unavailable and the 5 ms gate may not be met

#### Scenario: No warning emitted for fastrtps
- **WHEN** `RMW_IMPLEMENTATION=rmw_fastrtps_cpp` is set and the node activates
- **THEN** no WARN log is emitted regarding loaned message availability

### Requirement: Loaned vs copy-based path explicitly logged
C3 PerceptionNode SHALL log which subscription path was established — loaned or copy-based — as a distinct `RCLCPP_INFO` line in `on_activate()`, separate from the RMW name log.

#### Scenario: Loaned path logged when available
- **WHEN** the loaned subscription is created successfully
- **THEN** the log contains a line explicitly stating the loaned (zero-copy) path is active

#### Scenario: Copy-based fallback reason logged
- **WHEN** the loaned subscription throws and the copy-based fallback is used
- **THEN** the log contains the exception reason and a line stating the copy-based path is active
