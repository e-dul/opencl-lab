"""
C3 Perception Node — composable node launch file.

PerceptionNode and PointCloudPublisher run in the same ComposableNodeContainer.
RViz2 is launched as a plain Node (not composable — RViz uses Qt which cannot
run inside a component container).

Usage:
    ros2 launch perception_node perception_node.launch.py
    ros2 launch perception_node perception_node.launch.py use_rviz:=false
    ros2 launch perception_node perception_node.launch.py auto_activate:=false
    ros2 launch perception_node perception_node.launch.py gpu:=NVIDIA scene:=mixed

When auto_activate:=false the node stays INACTIVE after configure.  Activate manually:
    ros2 lifecycle set /perception_node activate
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode


def generate_launch_description():
    pkg_share  = get_package_share_directory('perception_node')
    yaml_path  = os.path.join(pkg_share, 'config', 'perception_node.yaml')
    rviz_path  = os.path.join(pkg_share, 'config', 'perception_node.rviz')

    auto_activate    = LaunchConfiguration('auto_activate')
    ground_z         = LaunchConfiguration('ground_z')
    min_intensity    = LaunchConfiguration('min_intensity')
    max_points       = LaunchConfiguration('max_points')
    use_double_buffer = LaunchConfiguration('use_double_buffer')
    topic            = LaunchConfiguration('topic')
    scene            = LaunchConfiguration('scene')
    hz               = LaunchConfiguration('hz')
    points           = LaunchConfiguration('points')
    gpu              = LaunchConfiguration('gpu')
    use_rviz         = LaunchConfiguration('use_rviz')

    return LaunchDescription([
        DeclareLaunchArgument('auto_activate',     default_value='true',
                              description='Self-activate after configure'),
        DeclareLaunchArgument('ground_z',          default_value='0.2',
                              description='Ground-plane Z threshold (metres)'),
        DeclareLaunchArgument('min_intensity',     default_value='10.0',
                              description='Minimum intensity threshold'),
        DeclareLaunchArgument('max_points',        default_value='100000',
                              description='Pre-allocated buffer size (points)'),
        DeclareLaunchArgument('use_double_buffer', default_value='false',
                              description='Enable non-blocking double-buffer pipeline'),
        DeclareLaunchArgument('topic',             default_value='/points',
                              description='PointCloud2 input topic'),
        DeclareLaunchArgument('scene',             default_value='grid',
                              description='Publisher scene: grid | mixed'),
        DeclareLaunchArgument('hz',                default_value='200',
                              description='Publisher rate in Hz'),
        DeclareLaunchArgument('points',            default_value='100000',
                              description='Points per publisher message'),
        DeclareLaunchArgument('gpu',               default_value='',
                              description='GPU vendor substring (e.g. NVIDIA, AMD)'),
        DeclareLaunchArgument('use_rviz',          default_value='true',
                              description='Launch RViz2 for visualisation'),

        ComposableNodeContainer(
            name='perception_container',
            namespace='',
            package='rclcpp_components',
            executable='component_container',
            additional_env={'GPU': gpu},
            composable_node_descriptions=[
                ComposableNode(
                    package='perception_node',
                    plugin='perception_node::PerceptionNode',
                    name='perception_node',
                    parameters=[yaml_path, {
                        'auto_activate':     auto_activate,
                        'ground_z':          ground_z,
                        'min_intensity':     min_intensity,
                        'max_points':        max_points,
                        'use_double_buffer': use_double_buffer,
                        'topic':             topic,
                    }],
                    extra_arguments=[{'use_intra_process_comms': True}],
                ),
                ComposableNode(
                    package='perception_node',
                    plugin='perception_node::PointCloudPublisher',
                    name='point_cloud_publisher',
                    parameters=[yaml_path, {
                        'topic':  topic,
                        'scene':  scene,
                        'hz':     hz,
                        'points': points,
                    }],
                    extra_arguments=[{'use_intra_process_comms': True}],
                ),
            ],
            output='screen',
        ),

        # WHY plain Node (not composable): RViz2 uses Qt and cannot run inside
        # a component container. A separate process is required.
        Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            arguments=['-d', rviz_path],
            condition=IfCondition(use_rviz),
            output='screen',
        ),
    ])
