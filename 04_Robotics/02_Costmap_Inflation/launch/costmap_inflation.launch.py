"""
C2 Costmap Inflation — composable node launch file.

CostmapNode and MapPublisher run in the same ComposableNodeContainer with
use_intra_process_comms=True, delivering OccupancyGrid as shared_ptr with
zero DDS serialisation.

Usage:
    ros2 launch costmap_inflation costmap_inflation.launch.py
    ros2 launch costmap_inflation costmap_inflation.launch.py map_path:=/path/to/warehouse.pgm
    ros2 launch costmap_inflation costmap_inflation.launch.py auto_activate:=false

When auto_activate:=false the node stays INACTIVE after configure.  Activate manually:
    ros2 lifecycle set /costmap_node activate
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    pkg_share = get_package_share_directory('costmap_inflation')
    yaml_path = os.path.join(pkg_share, 'config', 'costmap_inflation.yaml')

    auto_activate    = LaunchConfiguration('auto_activate')
    inflation_radius = LaunchConfiguration('inflation_radius')
    resolution       = LaunchConfiguration('resolution')
    decay            = LaunchConfiguration('decay')
    map_path         = LaunchConfiguration('map_path')
    gpu              = LaunchConfiguration('gpu')

    return LaunchDescription([
        DeclareLaunchArgument('auto_activate',    default_value='true',
                              description='Self-activate after configure'),
        DeclareLaunchArgument('inflation_radius', default_value='0.5',
                              description='Obstacle inflation radius in metres'),
        DeclareLaunchArgument('resolution',       default_value='0.05',
                              description='Metres per cell'),
        DeclareLaunchArgument('decay',            default_value='3.0',
                              description='Exponential cost decay rate'),
        DeclareLaunchArgument('map_path',         default_value='',
                              description='Path to .pgm occupancy grid (empty = synthetic)'),
        DeclareLaunchArgument('gpu',              default_value='',
                              description='GPU vendor substring (e.g. NVIDIA, AMD)'),

        ComposableNodeContainer(
            name='costmap_inflation_container',
            namespace='',
            package='rclcpp_components',
            executable='component_container',
            additional_env={'GPU': gpu},
            composable_node_descriptions=[
                ComposableNode(
                    package='costmap_inflation',
                    plugin='costmap_inflation::CostmapNode',
                    name='costmap_node',
                    parameters=[yaml_path, {
                        'auto_activate':    auto_activate,
                        'inflation_radius': inflation_radius,
                        'resolution':       resolution,
                        'decay':            decay,
                    }],
                    extra_arguments=[{'use_intra_process_comms': True}],
                ),
                ComposableNode(
                    package='costmap_inflation',
                    plugin='costmap_inflation::MapPublisher',
                    name='map_publisher',
                    parameters=[yaml_path, {
                        'map_path': map_path,
                    }],
                    extra_arguments=[{'use_intra_process_comms': True}],
                ),
            ],
            output='screen',
        ),
    ])
