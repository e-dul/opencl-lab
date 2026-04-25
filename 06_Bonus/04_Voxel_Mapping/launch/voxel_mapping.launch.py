"""
4.5 Voxel Mapping — composable node launch file.

VoxelMappingNode and VoxelCloudPublisher run in the same ComposableNodeContainer.
RViz2 is launched as a plain Node (not composable — RViz uses Qt which cannot
run inside a component container).

Usage:
    ros2 launch voxel_mapping voxel_mapping.launch.py
    ros2 launch voxel_mapping voxel_mapping.launch.py use_rviz:=false
    ros2 launch voxel_mapping voxel_mapping.launch.py scene:=dynamic gpu:=NVIDIA
    ros2 launch voxel_mapping voxel_mapping.launch.py enable_flip_filter:=true flip_threshold:=3
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
    pkg_share  = get_package_share_directory('voxel_mapping')
    yaml_path  = os.path.join(pkg_share, 'config', 'voxel_mapping.yaml')
    rviz_path  = os.path.join(pkg_share, 'config', 'voxel_mapping.rviz')

    topic              = LaunchConfiguration('topic')
    resolution         = LaunchConfiguration('resolution')
    output             = LaunchConfiguration('output')
    enable_flip_filter = LaunchConfiguration('enable_flip_filter')
    flip_threshold     = LaunchConfiguration('flip_threshold')
    scene              = LaunchConfiguration('scene')
    hz                 = LaunchConfiguration('hz')
    points             = LaunchConfiguration('points')
    frames             = LaunchConfiguration('frames')
    move_speed         = LaunchConfiguration('move_speed')
    gpu                = LaunchConfiguration('gpu')
    use_rviz           = LaunchConfiguration('use_rviz')

    return LaunchDescription([
        DeclareLaunchArgument('topic',              default_value='/points',
                              description='PointCloud2 topic to subscribe/publish on'),
        DeclareLaunchArgument('resolution',         default_value='0.1',
                              description='Voxel size in metres'),
        DeclareLaunchArgument('output',             default_value='output_voxel_slice.bmp',
                              description='Output BMP path for top-down slice'),
        DeclareLaunchArgument('enable_flip_filter', default_value='false',
                              description='Enable dynamic-object flip-count filter'),
        DeclareLaunchArgument('flip_threshold',     default_value='5',
                              description='Flip count threshold for dynamic object removal'),
        DeclareLaunchArgument('scene',              default_value='static',
                              description='Publisher scene: static | dynamic'),
        DeclareLaunchArgument('hz',                 default_value='10.0',
                              description='Publisher rate in Hz'),
        DeclareLaunchArgument('points',             default_value='10000',
                              description='Points per publisher message'),
        DeclareLaunchArgument('frames',             default_value='0',
                              description='Stop after N frames (0 = infinite)'),
        DeclareLaunchArgument('move_speed',         default_value='0.05',
                              description='Orbit angle increment per frame (rad, dynamic scene only)'),
        DeclareLaunchArgument('gpu',                default_value='',
                              description='GPU vendor substring (e.g. NVIDIA, AMD)'),
        DeclareLaunchArgument('use_rviz',           default_value='true',
                              description='Launch RViz2 for visualisation'),

        ComposableNodeContainer(
            name='voxel_mapping_container',
            namespace='',
            package='rclcpp_components',
            executable='component_container',
            additional_env={'GPU': gpu},
            composable_node_descriptions=[
                ComposableNode(
                    package='voxel_mapping',
                    plugin='voxel_mapping::VoxelMappingNode',
                    name='voxel_mapping',
                    parameters=[yaml_path, {
                        'topic':              topic,
                        'resolution':         resolution,
                        'output':             output,
                        'enable_flip_filter': enable_flip_filter,
                        'flip_threshold':     flip_threshold,
                    }],
                    extra_arguments=[{'use_intra_process_comms': True}],
                ),
                ComposableNode(
                    package='voxel_mapping',
                    plugin='voxel_mapping::VoxelCloudPublisher',
                    name='voxel_point_cloud_publisher',
                    parameters=[yaml_path, {
                        'topic':      topic,
                        'scene':      scene,
                        'hz':         hz,
                        'points':     points,
                        'frames':     frames,
                        'move_speed': move_speed,
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
