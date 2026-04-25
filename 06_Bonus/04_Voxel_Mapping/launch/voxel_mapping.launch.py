"""
4.5 Voxel Mapping — composable node launch file.

VoxelMappingNode and VoxelCloudPublisher run in the same ComposableNodeContainer.
RViz2 is launched as a plain Node (not composable — RViz uses Qt which cannot
run inside a component container).

Pass bag:=<path> to replay from a rosbag2 bag instead: VoxelCloudPublisher is
dropped and 'ros2 bag play <path>' is started as a side process. RViz is always
included (controlled separately by use_rviz).

Usage:
    ros2 launch voxel_mapping voxel_mapping.launch.py
    ros2 launch voxel_mapping voxel_mapping.launch.py use_rviz:=false
    ros2 launch voxel_mapping voxel_mapping.launch.py scene:=dynamic gpu:=NVIDIA
    ros2 launch voxel_mapping voxel_mapping.launch.py enable_flip_filter:=true flip_threshold:=3
    ros2 launch voxel_mapping voxel_mapping.launch.py bag:=voxel_bag
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, OpaqueFunction
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import ComposableNodeContainer, Node
from launch_ros.descriptions import ComposableNode


def launch_setup(context, *args, **kwargs):
    # WHY OpaqueFunction: ComposableNode has no condition= support, so
    # VoxelCloudPublisher cannot be dropped with IfCondition. OpaqueFunction
    # resolves 'bag' at launch time and builds the composable list with plain
    # Python if/else — the cleanest way to keep the container DRY.
    pkg_share = get_package_share_directory('voxel_mapping')
    yaml_path = os.path.join(pkg_share, 'config', 'voxel_mapping.yaml')
    rviz_path = os.path.join(pkg_share, 'config', 'voxel_mapping.rviz')

    bag_path           = LaunchConfiguration('bag').perform(context)
    topic              = LaunchConfiguration('topic').perform(context)
    resolution         = LaunchConfiguration('resolution').perform(context)
    output             = LaunchConfiguration('output').perform(context)
    enable_flip_filter = LaunchConfiguration('enable_flip_filter').perform(context)
    flip_threshold     = LaunchConfiguration('flip_threshold').perform(context)
    scene              = LaunchConfiguration('scene').perform(context)
    hz                 = LaunchConfiguration('hz').perform(context)
    points             = LaunchConfiguration('points').perform(context)
    frames             = LaunchConfiguration('frames').perform(context)
    move_speed         = LaunchConfiguration('move_speed').perform(context)
    gpu                = LaunchConfiguration('gpu').perform(context)
    use_rviz           = LaunchConfiguration('use_rviz').perform(context)

    voxel_node = ComposableNode(
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
    )

    cloud_publisher = ComposableNode(
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
    )

    nodes = [voxel_node] if bag_path else [voxel_node, cloud_publisher]

    container = ComposableNodeContainer(
        name='voxel_mapping_container',
        namespace='',
        package='rclcpp_components',
        executable='component_container',
        additional_env={'GPU': gpu},
        composable_node_descriptions=nodes,
        output='screen',
    )

    # WHY plain Node (not composable): RViz2 uses Qt and cannot run inside
    # a component container. A separate process is required.
    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        arguments=['-d', rviz_path],
        condition=IfCondition(use_rviz),
        output='screen',
    )

    actions = [container, rviz_node]
    if bag_path:
        actions.append(ExecuteProcess(
            cmd=['ros2', 'bag', 'play', bag_path],
            output='screen',
        ))
    return actions


def generate_launch_description():
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
        DeclareLaunchArgument('bag',                default_value='',
                              description='Path to a rosbag2 bag directory; '
                                          'suppresses the synthetic publisher when set.'),

        OpaqueFunction(function=launch_setup),
    ])
