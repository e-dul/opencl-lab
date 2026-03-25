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
    add_subdirectory(02_Multimedia/A1_OpenCV_Interop)
    add_subdirectory(02_Multimedia/A2_YUV_Pipeline)
    add_subdirectory(02_Multimedia/A2b_YUYV_Extension)
    add_subdirectory(02_Multimedia/A3_1_OpenCV_DNN)
    add_subdirectory(02_Multimedia/A3_2_OpenVINO_GPU)
    add_subdirectory(02_Multimedia/A4_Smart_Webcam)
    add_subdirectory(02_Multimedia/A5_Privacy_Mode)
    add_subdirectory(02_Multimedia/FFmpeg_Pipeline)
    add_subdirectory(02_Multimedia/SoftISP)
endif()

# Module 3 — Graphics & HPC
if(BUILD_GRAPHICSHPC)
    add_subdirectory(03_GraphicsHPC/B2_Ray_Tracer_Basic)
    add_subdirectory(03_GraphicsHPC/B3_Ray_Tracer_BVH)
    add_subdirectory(03_GraphicsHPC/B3_Ray_Tracer_BVH_Dynamic)
endif()

# Module 4 — Robotics / ROS2
if(BUILD_ROBOTICS)
    add_subdirectory(04_Robotics/C1_Node_Acceleration)
    add_subdirectory(04_Robotics/C2_Costmap_Inflation)
    add_subdirectory(04_Robotics/C3_Perception_Node)
endif()

# Module 5 — Toolbox (OpenCL_vs_CUDA is docs-only, no build target)
if(BUILD_TOOLBOX)
    add_subdirectory(05_Toolbox/ZeroCopy)
    add_subdirectory(05_Toolbox/CoalescedAccess)
    add_subdirectory(05_Toolbox/ThreadDivergence)
    add_subdirectory(05_Toolbox/LocalMemory)
    add_subdirectory(05_Toolbox/AsyncMultiThread)
    add_subdirectory(05_Toolbox/SyncAtomics)
    add_subdirectory(05_Toolbox/WorkGroupSizing)
    add_subdirectory(05_Toolbox/FastMath)
    add_subdirectory(05_Toolbox/MultiGPU_Strategy)
    add_subdirectory(05_Toolbox/SVM)
    add_subdirectory(05_Toolbox/SVM_Theory)
    add_subdirectory(05_Toolbox/GenericKernelTemplates)
    add_subdirectory(05_Toolbox/Debugging)
    add_subdirectory(05_Toolbox/Deployment)
endif()

# Module 6 — Bonus
if(BUILD_BONUS)
    add_subdirectory(06_Bonus/CLBlast_MatMul)
    add_subdirectory(06_Bonus/Device_Enqueue)
    add_subdirectory(06_Bonus/vkFFT_Audio)
    add_subdirectory(06_Bonus/Voxel_Mapping)
endif()
