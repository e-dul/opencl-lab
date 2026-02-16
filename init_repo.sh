#!/bin/bash
# init_repo.sh - Initialize Applied-OpenCL-Lab Structure (English Version)

set -e  # Exit on error

echo "🚀 Initializing Applied-OpenCL-Lab..."

# ============================================================================
# 1. MAIN STRUCTURE
# ============================================================================
echo "📁 Creating main structure..."

mkdir -p .ai/{tasks,archive}
mkdir -p design
mkdir -p common
mkdir -p vendor/{CL,stb}
mkdir -p scripts

# ============================================================================
# 2. MODULE 0: Setup
# ============================================================================
echo "📁 Module 0: Setup..."

mkdir -p 00_Setup/01_Smoke_Test/{src,kernels}

# ============================================================================
# 3. MODULE 1: Host API
# ============================================================================
echo "📁 Module 1: Host API..."

mkdir -p 01_Host_API/01_Visual_Kernel/{src,kernels,assets}
mkdir -p 01_Host_API/02_Visual_Kernel_Events/{src,kernels,assets}
mkdir -p 01_Host_API/03_Buffers_Layout/{src,kernels,assets}

# ============================================================================
# 4. MODULE 2: Projects
# ============================================================================
echo "📁 Module 2: Projects..."

# Path A: Multimedia
mkdir -p 02_Projects/A_Multimedia/01_OpenCV_Interop/{src,kernels,assets}
mkdir -p 02_Projects/A_Multimedia/02_YUV_Processing/{src,kernels,assets}
mkdir -p 02_Projects/A_Multimedia/03_AI_Inference/{src,kernels,assets}
mkdir -p 02_Projects/A_Multimedia/04_Webcam_Project/{src,include,kernels,assets}

# Path B: Graphics/HPC
mkdir -p 02_Projects/B_Graphics_HPC/01_CLBlast_Math/{src,kernels}
mkdir -p 02_Projects/B_Graphics_HPC/02_RayTracer_Basic/{src,include,kernels}
mkdir -p 02_Projects/B_Graphics_HPC/03_RayTracer_BVH/{src,include,kernels,assets}
mkdir -p 02_Projects/B_Graphics_HPC/04_Device_Enqueue/{src,kernels}

# Path C: Robotics/ROS2
mkdir -p 02_Projects/C_Robotics_ROS2/01_Node_Anatomy/{src,kernels}
mkdir -p 02_Projects/C_Robotics_ROS2/02_Costmap_Inflation/{src,kernels}
mkdir -p 02_Projects/C_Robotics_ROS2/03_Perception_Node/{src,include,kernels,assets}

# ============================================================================
# 5. TOOLBOX
# ============================================================================
echo "📁 99_Toolbox..."

mkdir -p 99_Toolbox/Zero_Copy_Demo/{src,kernels}
mkdir -p 99_Toolbox/Coalesced_Access/{src,kernels}
mkdir -p 99_Toolbox/Bank_Conflict_Test/{src,kernels}
mkdir -p 99_Toolbox/Thread_Divergence/{src,kernels}
mkdir -p 99_Toolbox/Local_Memory_Tile/{src,kernels}
mkdir -p 99_Toolbox/Register_Pressure/{src,kernels}
mkdir -p 99_Toolbox/Debugging_Oclgrind/{src,kernels}

# ============================================================================
# 6. ADDONS
# ============================================================================
echo "📁 Addons..."

mkdir -p Addons/vkFFT_Audio/{src,kernels,assets}
mkdir -p Addons/OpenCL_vs_CUDA
mkdir -p Addons/Deployment_Guide
mkdir -p Addons/SVM_Theory
mkdir -p Addons/Fusion_Voxels/{src,include,kernels,assets}
mkdir -p Addons/FFmpeg_Pipeline/{src,include,kernels,assets}
mkdir -p Addons/SoftISP_Debayering/{src,kernels,assets}

# ============================================================================
# 7. CONFIG FILES
# ============================================================================
echo "📝 Creating configuration files..."

# .gitignore
cat > .gitignore << 'EOF'
# Build artifacts
build/
cmake-build-*/
*.o
*.so
*.a
*.exe
*.lib
*.dll

# IDE
.vscode/
.idea/
*.swp
*.swo

# OS
.DS_Store
Thumbs.db

# Temporary
*.tmp
*.bak
*~
EOF

# .claudecodeignore
cat > .claudecodeignore << 'EOF'
# Build artifacts
build/
cmake-build-*/
*.o
*.so
*.a

# Git
.git/

# Assets (large media files)
assets/*.mp4
assets/*.avi
assets/*.mov
assets/*.pcd
assets/*.obj
*.onnx

# Vendor (do not analyze external code)
vendor/

# IDE
.vscode/
.idea/
*.swp
*.tmp
*.bak
EOF

# Main README.md
cat > README.md << 'EOF'
# Applied OpenCL Lab

A practical course on heterogeneous CPU/GPU system engineering based on OpenCL.

## Repository Structure

- `00_Setup/` - Module 0: Fundamentals and Environment
- `01_Host_API/` - Module 1: Host API and Feedback Loop
- `02_Projects/` - Module 2: Real-World Integration (3 Paths: A/B/C)
- `99_Toolbox/` - Optimization Toolbox (Shared Resources)
- `Addons/` - Module 4: Bonus content and Case Studies
- `design/` - Strategy Documents (Syllabus, Requirements)
- `.ai/` - AI Workspace (Protocols, Tasks, Memory)

## Quick Start

```bash
# Build all modules
cmake -B build
cmake --build build

# Build specific project
cd 01_Host_API/01_Visual_Kernel
cmake -B build
cmake --build build
```

## More Information

See `design/00-executive-summary.md` for the full project vision.
EOF

# Main CMakeLists.txt (Hybrid Approach)
cat > CMakeLists.txt << 'EOF'
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
EOF

# Common CMakeLists.txt
cat > common/CMakeLists.txt << 'EOF'
add_library(common INTERFACE)

target_include_directories(common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/vendor
)
EOF


# LICENSE (MIT)
cat > LICENSE << 'EOF'
MIT License

Copyright (c) 2026 Applied OpenCL Lab

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
EOF

# ============================================================================
# 8. MARKDOWN FILES (ENGLISH)
# ============================================================================
echo "📝 Creating markdown files..."

# .ai/ Protocols
cat > .ai/01-protocol-strategy-tactics.md << 'EOF'
# PROJECT OPERATING PROTOCOL: STRATEGY & TACTICS

## 1. Core Philosophy: Dual-Layer Context
To prevent context rot and hallucinations, we separate **Long-term Truth** from **Short-term Execution**.

### 🛡️ STRATEGY (The Design Layer)
- **Location:** `design/<feature_name>.md`
- **Purpose:** Single Source of Truth for Architecture, Vision, Key Decisions, and High-Level Status.
- **Rule:** NEVER implicitly change architecture defined here.

### ⚔️ TACTICS (The Task Layer)
- **Location:** `.ai/tasks/<id>_<task_name>.md`
- **Purpose:** Disposable instructions for atomic units of work.
- **Lifecycle:** Created -> Executed -> Archived (`.ai/archive/`).

## 2. The Operating Loop
1. **ANALYZE:** Read `design/*.md`. Identify next step.
2. **PLAN:** Create `.ai/tasks/XXX_name.md`.
3. **EXECUTE:** Implement code strictly based on the task file.
4. **SYNC & CLEAN:** Update design, move task to archive.
EOF

cat > .ai/02-communication-style.md << 'EOF'
# COMMUNICATION STYLE PROTOCOL

## 1. Core Principle: High Signal-to-Noise Ratio
- **ELIMINATE** conversational filler ("Here is the code", "I hope this helps").
- **ELIMINATE** summaries of code just provided.

## 2. Response Format
### For Code Tasks:
1. **Brief Analysis:** Bullet points listing critical decisions.
2. **The Code:** The implementation itself.
3. **Review:** Short list of verified edge cases or TODOs.

## 3. Diff-Driven Development
- **PREFER** showing `diff` or specific blocks.
- **AVOID** rewriting entire files unless necessary.
EOF

cat > .ai/03-safety.md << 'EOF'
# SAFETY & CRITICAL THINKING PROTOCOL

## 1. The "Don't Invent" Rule
- **Libraries:** Do not assume existence of libraries not present in context.
- **Context:** If a file is referenced but not read, **READ IT**.

## 2. C++ Specific Safety Rails
- **Memory:** Prefer RAII (`cl::Buffer`, `std::unique_ptr`) over raw `new/delete`.
- **Concurrency:** Assume multithreading. Protect shared state.
- **Performance:** Avoid deep copies of `cv::Mat`.

## 3. Self-Review Checklist
- [ ] Does this compile?
- [ ] Are edge cases handled?
- [ ] Did I break the public API?
EOF

cat > .ai/AI_GUIDELINES.md << 'EOF'
# AI Engineering Guidelines for Applied OpenCL Lab

## Project Philosophy
1. **Code-First:** Code is the primary knowledge carrier.
2. **Just-in-Time Learning:** Theory on demand.
3. **Education First:** Code must be readable by Mid-level engineers.
4. **Performance:** Always profile (Events) before optimizing.

## Architecture
- `common/` contains helper functions (error handling, image IO).
- Kernels (`.cl`) are separate files, loaded at runtime.
- Build system is CMake (standalone per module).
- Use RAII wrappers for OpenCL resources.

## Code Conventions
- Variables: `snake_case`
- Classes: `PascalCase`
- Constants: `UPPER_CASE`
- Always check OpenCL return codes.

## Communication Style
- Terse: No filler phrases.
- Diff-Driven: Show code changes.
- Direct: Start with the answer.
EOF

cat > .ai/MEMORY.md << 'EOF'
# Applied OpenCL Lab – Memory Log

## Design Decisions (Long-Term)
- **Build System:** CMake 3.18+, hybrid approach (Main + Standalone).
- **OpenCL Wrapper:** cl.hpp version 1.2 (for Nvidia compatibility).
- **C++ Standard:** C++17.

## Progress Tracking
- [ ] Module 0: Setup
- [ ] Module 1: Host API
- [ ] Module 2: Projects
- [ ] Toolbox
- [ ] Addons

## Style Guide (Coding Conventions)
- Naming: `snake_case` for variables, `PascalCase` for classes.
- Buffers: Always RAII (`cl::Buffer`).
- Comments: Explain "WHY", not "WHAT".

## Known Issues
[TODO: Add issues here]

## Session Notes
[TODO: Add notes after each session]
EOF

# design/
cat > design/00-executive-summary.md << 'EOF'
# Executive Summary – Applied OpenCL Lab

## Goal
Create a practical educational resource for heterogeneous CPU/GPU engineering.

## Philosophy
- **Zero-Bullshit Engineering:** No textbook definitions, just working Real-Time systems.
- **Code-First:** Just-in-Time Learning.
- **Hub & Spoke:** Projects (Spokes) link to Optimization Toolbox (Hub).

[See original PDF for full details]
EOF

cat > design/01-host-api.md << 'EOF'
# Module 1: Host API & Feedback Loop

## Objectives
- Understand C++ Wrapper API.
- Learn Profiling (Events) from day one.
- Understand Buffer Layouts.

## Roadmap
- [ ] 1.1 Visual Kernel (Basic)
- [ ] 1.2 Visual Kernel + Events
- [ ] 1.3 Buffers & Data Layout
EOF

cat > design/02-projects-core.md << 'EOF'
# Module 2: Real-World Integration

## Path A: Multimedia & AI
- Focus: Zero-Copy, OpenCV Interop, Edge AI.
- Project: AI Smart Webcam.

## Path B: Graphics & HPC
- Focus: Ray Tracing, BVH, Dynamic Parallelism.
- Project: Advanced Ray Tracer.

## Path C: Robotics & ROS 2
- Focus: Node Acceleration, Costmaps, Latency.
- Project: Accelerated Perception Node.
EOF

cat > design/03-optimization-toolbox.md << 'EOF'
# Optimization Toolbox

A centralized resource for optimization techniques.

## Tools
1. Memory Management (Zero-Copy, Coalescing)
2. Kernel Execution (Thread Divergence, Local Memory)
3. Debugging (Oclgrind, Profiling)
EOF

# Helper function for directory MD files
create_dir_md() {
    local path=$1
    local title=$2
    cat > "$path" << EOF
# $title

## Objective
[TODO: Describe objective]

## Contents
[TODO: List contents]

## Requirements
- OpenCL 1.2+
- CMake 3.18+
EOF
}

# Module 0
create_dir_md "00_Setup/Setup.md" "Module 0: Setup & Fundamentals"
create_dir_md "00_Setup/01_Smoke_Test/SmokeTest.md" "Smoke Test: Vector Add"

# Module 1
create_dir_md "01_Host_API/HostAPI.md" "Module 1: Host API"
create_dir_md "01_Host_API/01_Visual_Kernel/VisualKernel.md" "Visual Kernel (Basic)"
create_dir_md "01_Host_API/02_Visual_Kernel_Events/VisualKernelEvents.md" "Visual Kernel + Events"
create_dir_md "01_Host_API/03_Buffers_Layout/BuffersLayout.md" "Buffers & Data Layout"

# Module 2
create_dir_md "02_Projects/Projects.md" "Module 2: Real-World Integration"
create_dir_md "02_Projects/A_Multimedia/Multimedia.md" "Path A: Multimedia"
create_dir_md "02_Projects/B_Graphics_HPC/GraphicsHPC.md" "Path B: Graphics/HPC"
create_dir_md "02_Projects/C_Robotics_ROS2/RoboticsROS2.md" "Path C: Robotics/ROS 2"

# Toolbox
create_dir_md "99_Toolbox/Toolbox.md" "Optimization Toolbox"

# Addons
create_dir_md "Addons/Addons.md" "Module 4: Addons"

# ============================================================================
# 9. SCRIPTS
# ============================================================================
echo "📝 Creating utility scripts..."

cat > scripts/build_all.sh << 'EOF'
#!/bin/bash
# Build all modules

set -e

echo "🔨 Building project..."

# Configure with default options (Mod 0 + 1)
cmake -B build     -DBUILD_MODULE_0=ON     -DBUILD_MODULE_1=ON     -DBUILD_MODULE_2=OFF     -DBUILD_TOOLBOX=OFF     -DBUILD_ADDONS=OFF

cmake --build build -j$(nproc)

echo "✅ Build complete!"
EOF

chmod +x scripts/build_all.sh

cat > scripts/test_all.sh << 'EOF'
#!/bin/bash
# Run all tests

set -e

echo "🧪 Running tests..."

cd build
ctest --output-on-failure

echo "✅ Tests complete!"
EOF

chmod +x scripts/test_all.sh

# ============================================================================
# 10. GIT INIT
# ============================================================================
echo "🔧 Initializing Git..."

git init
git add .
git commit -m "Initial commit: Applied OpenCL Lab structure (English)"

# ============================================================================
# SUMMARY
# ============================================================================
echo ""
echo "✅ Repository initialized successfully!"
echo ""
echo "📂 Next steps:"
echo "   1. Add vendor dependencies (cl.hpp, stb_image.h) to 'vendor/'"
echo "   2. Review 'design/00-executive-summary.md'"
echo "   3. Start with '00_Setup'"
echo ""
echo "🚀 Ready for Claude Code!"
