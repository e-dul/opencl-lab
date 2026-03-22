cmake_minimum_required(VERSION 3.18)
project(AppliedOpenCLLab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Module Build Options
option(BUILD_MODULE_0 "Build Module 0: Setup" ON)
option(BUILD_MODULE_1 "Build Module 1: Host API" OFF)
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
    add_subdirectory(01_Host_API/03_Buffer_Flags)
endif()

# Module 2
if(BUILD_MODULE_2)
    # Sub-options for Paths
    option(BUILD_PATH_A "Build Path A: Multimedia" OFF)
    option(BUILD_PATH_B "Build Path B: Graphics/HPC" OFF)
    option(BUILD_PATH_C "Build Path C: Robotics/ROS2" OFF)

    if(BUILD_PATH_A)
        add_subdirectory(02_Projects/A_Multimedia/A1_OpenCV_Interop)
        add_subdirectory(02_Projects/A_Multimedia/A2_YUV_Pipeline)
        add_subdirectory(02_Projects/A_Multimedia/A2b_YUYV_Extension)
        add_subdirectory(02_Projects/A_Multimedia/A3_1_OpenCV_DNN)
        add_subdirectory(02_Projects/A_Multimedia/A3_2_OpenVINO_GPU)
        add_subdirectory(02_Projects/A_Multimedia/A4_Smart_Webcam)
        add_subdirectory(02_Projects/A_Multimedia/A5_Privacy_Mode)
    endif()

    if(BUILD_PATH_B)
        add_subdirectory(02_Projects/B_Graphics_HPC/B1_CLBlast_MatMul)
        add_subdirectory(02_Projects/B_Graphics_HPC/B2_Ray_Tracer_Basic)
        add_subdirectory(02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH)
        add_subdirectory(02_Projects/B_Graphics_HPC/B3_Ray_Tracer_BVH_Dynamic)
        add_subdirectory(02_Projects/B_Graphics_HPC/B4_Device_Enqueue)
    endif()

    if(BUILD_PATH_C)
        add_subdirectory(02_Projects/C_Robotics_ROS2/C1_Node_Acceleration)
        add_subdirectory(02_Projects/C_Robotics_ROS2/C2_Costmap_Inflation)
        add_subdirectory(02_Projects/C_Robotics_ROS2/C3_Perception_Node)
    endif()
endif()

# Toolbox
if(BUILD_TOOLBOX)
    add_subdirectory(99_Toolbox/ZeroCopy)
    add_subdirectory(99_Toolbox/CoalescedAccess)
    add_subdirectory(99_Toolbox/ThreadDivergence)
    add_subdirectory(99_Toolbox/LocalMemory)
    add_subdirectory(99_Toolbox/AsyncMultiThread)
    add_subdirectory(99_Toolbox/SyncAtomics)
    add_subdirectory(99_Toolbox/WorkGroupSizing)
    add_subdirectory(99_Toolbox/FastMath)
    add_subdirectory(99_Toolbox/MultiGPU_Strategy)
    add_subdirectory(99_Toolbox/SVM)
    add_subdirectory(99_Toolbox/GenericKernelTemplates)
    add_subdirectory(99_Toolbox/Debugging)
endif()

# Addons (4_2_OpenCL_vs_CUDA is docs-only, no build target)
if(BUILD_ADDONS)
    add_subdirectory(04_Addons/4_1_vkFFT_Audio)
    add_subdirectory(04_Addons/4_3_Deployment)
    add_subdirectory(04_Addons/4_4_SVM_Theory)
    add_subdirectory(04_Addons/4_5_Voxel_Mapping)
    add_subdirectory(04_Addons/4_6_FFmpeg_Pipeline)
    add_subdirectory(04_Addons/4_7_SoftISP)
endif()
