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
