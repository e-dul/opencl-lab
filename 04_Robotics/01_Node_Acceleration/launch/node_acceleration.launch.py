"""
C1 Node Acceleration — composable node launch file.

AccelNode runs in a ComposableNodeContainer. By default SyntheticPublisher
is co-located in the same container (intra-process, zero DDS serialisation).
Pass bag:=<path> to replay from a rosbag2 bag instead: the synthetic
publisher is dropped and 'ros2 bag play <path>' is started as a side process.

Usage:
    ros2 launch node_acceleration node_acceleration.launch.py
    ros2 launch node_acceleration node_acceleration.launch.py auto_activate:=false
    ros2 launch node_acceleration node_acceleration.launch.py iterations:=20 buffer_size:=2097152
    ros2 launch node_acceleration node_acceleration.launch.py bag:=accel_bag

When auto_activate:=false the node stays INACTIVE after configure.  Activate manually:
    ros2 lifecycle set /accel_node activate
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def launch_setup(context, *args, **kwargs):
    # WHY OpaqueFunction: ComposableNode has no condition= support, so the
    # publisher cannot be dropped with IfCondition. OpaqueFunction resolves
    # 'bag' at launch time and builds the composable list with plain Python
    # if/else — the cleanest way to keep the container DRY.
    pkg_share = get_package_share_directory('node_acceleration')
    yaml_path = os.path.join(pkg_share, 'config', 'node_acceleration.yaml')

    bag_path     = LaunchConfiguration('bag').perform(context)
    auto_activate = LaunchConfiguration('auto_activate').perform(context)
    iterations    = LaunchConfiguration('iterations').perform(context)
    buffer_size   = LaunchConfiguration('buffer_size').perform(context)
    gpu           = LaunchConfiguration('gpu').perform(context)

    accel_node = ComposableNode(
        package='node_acceleration',
        plugin='node_acceleration::AccelNode',
        name='accel_node',
        parameters=[yaml_path, {
            'auto_activate': auto_activate,
            'iterations':    iterations,
            'buffer_size':   buffer_size,
        }],
        # WHY use_intra_process_comms: when the publisher is in the same
        # container, Float32MultiArray is delivered as shared_ptr — zero DDS
        # serialisation. Disabled automatically when bag replay is used (the
        # publisher is absent and messages arrive over DDS from ros2 bag play).
        extra_arguments=[{'use_intra_process_comms': True}],
    )

    synthetic_publisher = ComposableNode(
        package='node_acceleration',
        plugin='node_acceleration::SyntheticPublisher',
        name='synthetic_publisher',
        parameters=[yaml_path, {
            'iterations':  iterations,
            'buffer_size': buffer_size,
        }],
        extra_arguments=[{'use_intra_process_comms': True}],
    )

    nodes = [accel_node] if bag_path else [accel_node, synthetic_publisher]

    container = ComposableNodeContainer(
        name='node_acceleration_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        # WHY additional_env: GPU env var selects the OpenCL device vendor;
        # must be visible to the container process, not just the shell.
        additional_env={'GPU': gpu},
        composable_node_descriptions=nodes,
        output='screen',
    )

    actions = [container]
    if bag_path:
        actions.append(ExecuteProcess(
            cmd=['ros2', 'bag', 'play', bag_path],
            output='screen',
        ))
    return actions


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('auto_activate', default_value='true',
                              description='Self-activate after configure (false = wait for ros2 lifecycle)'),
        DeclareLaunchArgument('iterations',    default_value='10',
                              description='Number of callback cycles to run'),
        DeclareLaunchArgument('buffer_size',   default_value='1048576',
                              description='Floats per message'),
        DeclareLaunchArgument('gpu',           default_value='',
                              description='GPU vendor substring for device selection (e.g. NVIDIA, AMD)'),
        DeclareLaunchArgument('bag',           default_value='',
                              description='Path to a rosbag2 bag directory; '
                                          'suppresses the synthetic publisher when set.'),

        OpaqueFunction(function=launch_setup),
    ])
