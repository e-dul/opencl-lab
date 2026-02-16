cmake_minimum_required(VERSION 3.18)
project(AppliedOpenCLLab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Module Build Options
option(BUILD_MODULE_0 "Build Module 0: Setup" ON)
option(BUILD_MODULE_1 "Build Module 1: Host API" ON)
option(BUILD_MODULE_2 "Build Module 2: Projects" OFF)
option(BUILD_TOOLBOX "Build Optimization Toolbox" OFF)
option(BUILD_ADDONS "Build Addons" OFF)

# Common Library (Header-only)
add_subdirectory(common)

# Module 0
if(BUILD_MODULE_0)
    add_subdirectory(00_Setup/01_Smoke_Test)
endif()

# Module 1
if(BUILD_MODULE_1)
    add_subdirectory(01_Host_API/01_Visual_Kernel)
    add_subdirectory(01_Host_API/02_Visual_Kernel_Events)
    add_subdirectory(01_Host_API/03_Buffers_Layout)
endif()

# Module 2
if(BUILD_MODULE_2)
    # Sub-options for Paths
    option(BUILD_PATH_A "Build Path A: Multimedia" OFF)
    option(BUILD_PATH_B "Build Path B: Graphics/HPC" OFF)
    option(BUILD_PATH_C "Build Path C: Robotics/ROS2" OFF)

    if(BUILD_PATH_A)
        add_subdirectory(02_Projects/A_Multimedia/01_OpenCV_Interop)
        add_subdirectory(02_Projects/A_Multimedia/02_YUV_Processing)
        add_subdirectory(02_Projects/A_Multimedia/03_AI_Inference)
        add_subdirectory(02_Projects/A_Multimedia/04_Webcam_Project)
    endif()

    if(BUILD_PATH_B)
        add_subdirectory(02_Projects/B_Graphics_HPC/01_CLBlast_Math)
        add_subdirectory(02_Projects/B_Graphics_HPC/02_RayTracer_Basic)
        add_subdirectory(02_Projects/B_Graphics_HPC/03_RayTracer_BVH)
        add_subdirectory(02_Projects/B_Graphics_HPC/04_Device_Enqueue)
    endif()

    if(BUILD_PATH_C)
        add_subdirectory(02_Projects/C_Robotics_ROS2/01_Node_Anatomy)
        add_subdirectory(02_Projects/C_Robotics_ROS2/02_Costmap_Inflation)
        add_subdirectory(02_Projects/C_Robotics_ROS2/03_Perception_Node)
    endif()
endif()

# Toolbox
if(BUILD_TOOLBOX)
    add_subdirectory(99_Toolbox/Zero_Copy_Demo)
    add_subdirectory(99_Toolbox/Coalesced_Access)
    add_subdirectory(99_Toolbox/Bank_Conflict_Test)
    add_subdirectory(99_Toolbox/Thread_Divergence)
    add_subdirectory(99_Toolbox/Local_Memory_Tile)
    add_subdirectory(99_Toolbox/Register_Pressure)
    add_subdirectory(99_Toolbox/Debugging_Oclgrind)
endif()

# Addons
if(BUILD_ADDONS)
    add_subdirectory(Addons/vkFFT_Audio)
    add_subdirectory(Addons/Fusion_Voxels)
    add_subdirectory(Addons/FFmpeg_Pipeline)
    add_subdirectory(Addons/SoftISP_Debayering)
endif()
