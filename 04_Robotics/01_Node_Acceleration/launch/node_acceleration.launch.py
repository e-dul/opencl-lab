"""
C1 Node Acceleration — composable node launch file.

Both AccelNode and SyntheticPublisher run in the same ComposableNodeContainer
with use_intra_process_comms=True, eliminating DDS serialisation overhead for
the Float32MultiArray messages on /raw_floats.

Usage:
    ros2 launch node_acceleration node_acceleration.launch.py
    ros2 launch node_acceleration node_acceleration.launch.py auto_activate:=false
    ros2 launch node_acceleration node_acceleration.launch.py iterations:=20 buffer_size:=2097152

When auto_activate:=false the node stays INACTIVE after configure.  Activate manually:
    ros2 lifecycle set /accel_node activate
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    pkg_share = get_package_share_directory('node_acceleration')
    yaml_path = os.path.join(pkg_share, 'config', 'node_acceleration.yaml')

    auto_activate = LaunchConfiguration('auto_activate')
    iterations    = LaunchConfiguration('iterations')
    buffer_size   = LaunchConfiguration('buffer_size')
    gpu           = LaunchConfiguration('gpu')

    return LaunchDescription([
        DeclareLaunchArgument('auto_activate', default_value='true',
                              description='Self-activate after configure (false = wait for ros2 lifecycle)'),
        DeclareLaunchArgument('iterations',    default_value='10',
                              description='Number of callback cycles to run'),
        DeclareLaunchArgument('buffer_size',   default_value='1048576',
                              description='Floats per message'),
        DeclareLaunchArgument('gpu',           default_value='',
                              description='GPU vendor substring for device selection (e.g. NVIDIA, AMD)'),

        ComposableNodeContainer(
            name='node_acceleration_container',
            namespace='',
            package='rclcpp_components',
            executable='component_container',
            # WHY additional_env: GPU env var selects the OpenCL device vendor;
            # must be visible to the container process, not just the shell.
            additional_env={'GPU': gpu},
            composable_node_descriptions=[
                ComposableNode(
                    package='node_acceleration',
                    plugin='node_acceleration::AccelNode',
                    name='accel_node',
                    parameters=[yaml_path, {
                        'auto_activate': auto_activate,
                        'iterations':    iterations,
                        'buffer_size':   buffer_size,
                    }],
                    # WHY use_intra_process_comms: both nodes in the same container
                    # deliver Float32MultiArray as shared_ptr — zero DDS serialisation.
                    extra_arguments=[{'use_intra_process_comms': True}],
                ),
                ComposableNode(
                    package='node_acceleration',
                    plugin='node_acceleration::SyntheticPublisher',
                    name='synthetic_publisher',
                    parameters=[yaml_path, {
                        'iterations':  iterations,
                        'buffer_size': buffer_size,
                    }],
                    extra_arguments=[{'use_intra_process_comms': True}],
                ),
            ],
            output='screen',
        ),
    ])
