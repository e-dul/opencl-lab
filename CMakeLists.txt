cmake_minimum_required(VERSION 3.18)
project(AppliedOpenCLLab LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Module Build Options
option(BUILD_MODULE_0    "Build Module 0: Setup"          ON)
option(BUILD_MODULE_1    "Build Module 1: Host API"        OFF)
option(BUILD_MULTIMEDIA  "Build Module 2: Multimedia"      OFF)
option(BUILD_GRAPHICSHPC "Build Module 3: Graphics & HPC"  OFF)
option(BUILD_ROBOTICS    "Build Module 4: Robotics/ROS2"   OFF)
option(BUILD_TOOLBOX     "Build Module 5: Toolbox"         OFF)
option(BUILD_BONUS       "Build Module 6: Bonus"           OFF)

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

# Module 2 — Multimedia
if(BUILD_MULTIMEDIA)
    add_subdirectory(02_Multimedia/01_OpenCV_Interop)
    add_subdirectory(02_Multimedia/02_YUV_Pipeline)
    add_subdirectory(02_Multimedia/03_YUYV_Extension)
    add_subdirectory(02_Multimedia/04_OpenCV_DNN)
    add_subdirectory(02_Multimedia/05_OpenVINO_GPU)
    add_subdirectory(02_Multimedia/06_Smart_Webcam)
    add_subdirectory(02_Multimedia/07_Privacy_Mode)
    add_subdirectory(02_Multimedia/08_FFmpeg_Pipeline)
    add_subdirectory(02_Multimedia/09_SoftISP)
endif()

# Module 3 — Graphics & HPC
if(BUILD_GRAPHICSHPC)
    add_subdirectory(03_GraphicsHPC/01_Ray_Tracer_Basic)
    add_subdirectory(03_GraphicsHPC/02_Ray_Tracer_BVH)
    add_subdirectory(03_GraphicsHPC/03_Ray_Tracer_BVH_Dynamic)
endif()

# Module 4 — Robotics / ROS2
if(BUILD_ROBOTICS)
    add_subdirectory(04_Robotics/01_Node_Acceleration)
    add_subdirectory(04_Robotics/02_Costmap_Inflation)
    add_subdirectory(04_Robotics/03_Perception_Node)
endif()

# Module 5 — Toolbox (09_OpenCL_vs_CUDA is docs-only, no build target)
if(BUILD_TOOLBOX)
    add_subdirectory(05_Toolbox/01_Local_Memory)
    add_subdirectory(05_Toolbox/02_Coalesced_Access)
    add_subdirectory(05_Toolbox/03_Debugging)
    add_subdirectory(05_Toolbox/04_Deployment)
    add_subdirectory(05_Toolbox/05_Fast_Math)
    add_subdirectory(05_Toolbox/06_Generic_Kernel_Templates)
    add_subdirectory(05_Toolbox/07_Global_Work_Offset)
    add_subdirectory(05_Toolbox/08_Multi_GPU_Strategy)
    add_subdirectory(05_Toolbox/10_Sub_Buffers_Partitioning)
    add_subdirectory(05_Toolbox/11_SVM_Theory)
    add_subdirectory(05_Toolbox/12_Sync_Atomics)
    add_subdirectory(05_Toolbox/13_Thread_Divergence)
    add_subdirectory(05_Toolbox/14_Work_Group_Sizing)
    add_subdirectory(05_Toolbox/15_Zero_Copy)
    add_subdirectory(05_Toolbox/16_Async_Multi_Thread)
endif()

# Module 6 — Bonus
if(BUILD_BONUS)
    add_subdirectory(06_Bonus/01_CLBlast_MatMul)
    add_subdirectory(06_Bonus/02_Device_Enqueue)
    add_subdirectory(06_Bonus/03_VkFFT_Audio)
    add_subdirectory(06_Bonus/04_Voxel_Mapping)
endif()
